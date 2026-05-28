// BLE 스캔 + 연결 — 노드 발견 → pending 큐 → 연결 수립
#include <Arduino.h>
#include "ble_internal.h"
#include "ble_connection.h"
#include "ble_data.h"
#include "../cmd/cmd_dispatcher.h"
#include "../util/thread_safe_queue.h"

// ──────────── 연결 대기 큐 ────────────
// 스캔에서 발견된 노드 주소를 여기 쌓아두고, processNextPending()에서 하나씩 연결
// 기존 pendingAddrs[] + pendingCount + 수동 시프트를 ThreadSafeQueue로 교체
static ThreadSafeQueue<NimBLEAddress, PENDING_MAX> pendingQueue;

// 이미 대기열에 있는 주소인지 중복 체크
static bool isAlreadyPending(const NimBLEAddress& addr) {
    return pendingQueue.contains([&](const NimBLEAddress& a) {
        return a == addr;
    });
}

// ──────────── NimBLE 콜백 ────────────

// 스캔 결과 콜백 — tempio 서비스 UUID를 광고하는 노드만 대기열에 추가
class ScanCB : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (!dev->isAdvertisingService(NimBLEUUID(TEMPIO_SERVICE_UUID))) return;
        auto addr = dev->getAddress();
        if (isAlreadyConnected(addr)) return;  // 이미 연결됨
        if (isAlreadyPending(addr))   return;  // 이미 대기열에 있음
        if (findEmptySlot() < 0)      return;  // 슬롯 풀
        if (pendingQueue.full())      return;  // 대기열 꽉 참

        Serial.printf("found: %s  rssi=%d\n",
                      addr.toString().c_str(), dev->getRSSI());
        pendingQueue.push(addr);
    }
};

// 연결/해제 콜백 — NimBLE가 연결 상태 변할 때 호출
class ClientCB : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* c) override {
        Serial.printf("connected: %s\n", c->getPeerAddress().toString().c_str());
    }

    void onDisconnect(NimBLEClient* c, int reason) override {
        int slot = findSlotByClient(c);
        if (slot >= 0) {
            Serial.printf("%s disconnected (reason=%d)\n",
                          nodes[slot].addr.toString().c_str(), reason);
            nodes[slot].used = false;
            nodes[slot].configChar = nullptr;
        }
        // LED 꺼짐 = 연결된 노드 0개, 재스캔 시작
        digitalWrite(LED_PIN, activeCount() > 0 ? LED_ON : !LED_ON);
        doScan = true;
    }
};

static ScanCB  scanCb;
static ClientCB clientCb;

// ──────────── 연결 헬퍼 ────────────

// NimBLE 2.5.0 버그: deleteClient()가 heap 크래시 → 삭제 대신 빈 슬롯에 보관해서 재사용
static void stashClient(NimBLEClient* c) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].used && !nodes[i].client) {
            nodes[i].client = c;
            return;
        }
    }
}

// 연결 해제 후 클라이언트 객체를 재사용 풀에 반환
static void disconnectAndStash(NimBLEClient* c) {
    c->disconnect();
    stashClient(c);
}

// 재사용 가능한 클라이언트 가져오기, 없으면 새로 생성
static NimBLEClient* acquireClient() {
    auto* c = findReusableClient();
    if (!c) c = NimBLEDevice::createClient();
    if (c) c->setClientCallbacks(&clientCb);
    return c;
}

// 서비스 탐색 + DATA 특성 구독
static NimBLERemoteService* discoverAndSubscribe(NimBLEClient* client) {
    auto* svc = client->getService(TEMPIO_SERVICE_UUID);
    if (!svc) return nullptr;
    auto* dataChar = svc->getCharacteristic(TEMPIO_CHAR_DATA_UUID);
    if (!dataChar || !dataChar->canNotify()) return nullptr;
    dataChar->subscribe(true, onDataNotify);
    return svc;
}

// 슬롯 등록 + HUB_READY 전송
static void registerNode(int slot, NimBLEClient* client,
                         const NimBLEAddress& addr, NimBLERemoteService* svc) {
    nodes[slot].client     = client;
    nodes[slot].configChar = svc->getCharacteristic(TEMPIO_CHAR_CONFIG_UUID);
    nodes[slot].addr       = addr;
    nodes[slot].used       = true;

    // MTU 협상 완료 대기 — 이 딜레이 없이 write하면 BLE 스택이 불안정
    delay(POST_CONNECT_DELAY_MS);
    HubReady ready;
    if (nodes[slot].configChar) {
        nodes[slot].configChar->writeValue(
            reinterpret_cast<const uint8_t*>(&ready), sizeof(ready));
    }
    Serial.printf("<< HUB_READY → %s\n", addr.toString().c_str());
    digitalWrite(LED_PIN, LED_ON);

    // 이 노드에 대기 중인 서버 명령이 있으면 지금 전송
    flushNodePending(addr.toString().c_str());
}

// 노드 연결 전체 흐름: 클라이언트 획득 → TCP 연결 → 서비스 탐색 → 슬롯 등록
static bool connectToNode(const NimBLEAddress& addr) {
    int slot = findEmptySlot();
    if (slot < 0) return false;

    auto* client = acquireClient();
    if (!client) return false;

    if (!client->connect(addr)) {
        disconnectAndStash(client);
        return false;
    }
    auto* svc = discoverAndSubscribe(client);
    if (!svc) {
        disconnectAndStash(client);
        return false;
    }
    registerNode(slot, client, addr, svc);
    return true;
}

// ──────────── 공개 API ────────────

void bleConnectionInit(NimBLEScan* pScan) {
    pScan->setScanCallbacks(&scanCb);
    pScan->setActiveScan(true);
    pScan->setInterval(BLE_SCAN_INTERVAL);
    pScan->setWindow(BLE_SCAN_WINDOW);
}

// 대기열 맨 앞 주소를 꺼내 연결 시도. 스캔 일시정지 → 연결 → 스캔 재개.
void processNextPending() {
    NimBLEAddress addr;
    if (!pendingQueue.pop(&addr)) return;  // 대기열 비었으면 즉시 리턴

    NimBLEDevice::getScan()->stop();
    connectToNode(addr);
    NimBLEDevice::getScan()->start(0, false);
}
