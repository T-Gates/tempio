#include <Arduino.h>
#include <NimBLEDevice.h>
#include <cstring>
#include "protocol.h"
#include "ble_central.h"

static constexpr uint8_t LED_PIN = 8;

static NimBLEClient* pClient = nullptr;
static NimBLERemoteCharacteristic* pConfigChar = nullptr;
// volatile: BLE 태스크와 loop 태스크에서 동시 접근하는 플래그
static volatile bool connected = false;
static volatile bool doConnect = false;
// 재스캔 요청도 콜백→loop 위임 (콜백 안에서 직접 스캔 시작하면 불안정)
static volatile bool doScan = false;
static NimBLEAddress targetAddr;
static unsigned long lastPrint = 0;

// ──────────── notify 수신 ────────────

static void onDataNotify(NimBLERemoteCharacteristic* c,
                         uint8_t* data, size_t len, bool isNotify) {
    if (len < 1) return;

    auto type = static_cast<MsgType>(data[0]);
    switch (type) {
        case MsgType::NODE_INFO: {
            if (len < sizeof(NodeInfo)) break;
            // memcpy로 정렬 안전하게 복사 (reinterpret_cast 대신)
            NodeInfo ni;
            memcpy(&ni, data, sizeof(ni));
            const char* typeName = (ni.node_type == NodeType::SENSOR) ? "sensor" : "ir";
            Serial.printf(">> NodeInfo: type=%s id=%u bat=%umV fw=%u.%u\n",
                          typeName, ni.node_id, ni.battery_mv,
                          ni.fw_major, ni.fw_minor);
            break;
        }
        case MsgType::SENSOR_DATA: {
            if (len < sizeof(SensorData)) break;
            SensorData sd;
            memcpy(&sd, data, sizeof(sd));
            Serial.printf(">> sensor: %.1f C  %.1f %%  ldr=%u  bat=%umV\n",
                          sd.temp, sd.humidity, sd.ldr, sd.battery_mv);
            break;
        }
        default:
            Serial.printf(">> unknown: 0x%02x (%u bytes)\n", data[0], len);
    }
}

// ──────────── 콜백 (static 인스턴스로 힙 할당 방지) ────────────

class ScanCB : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (dev->isAdvertisingService(NimBLEUUID(TEMPIO_SERVICE_UUID))) {
            Serial.printf("found: %s  rssi=%d\n",
                          dev->getAddress().toString().c_str(), dev->getRSSI());
            NimBLEDevice::getScan()->stop();
            targetAddr = dev->getAddress();
            doConnect = true;
        }
    }
};

class ClientCB : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* c) override {
        Serial.println("connected");
    }

    void onDisconnect(NimBLEClient* c, int reason) override {
        connected = false;
        pConfigChar = nullptr;
        digitalWrite(LED_PIN, HIGH);
        Serial.printf("disconnected (reason=%d)\n", reason);
        // 콜백 안에서 직접 스캔 시작하지 않고 loop에 위임
        doScan = true;
    }
};

// static 인스턴스 — new 대신 재사용해서 힙 단편화 방지
static ScanCB scanCb;
static ClientCB clientCb;

// ──────────── 연결·명령 ────────────

static bool connectToServer() {
    // 기존 클라이언트가 있으면 정리 (풀 소진 방지)
    if (pClient) {
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
    }

    pClient = NimBLEDevice::createClient();
    if (!pClient) {
        Serial.println("createClient failed");
        return false;
    }
    pClient->setClientCallbacks(&clientCb);

    if (!pClient->connect(targetAddr)) {
        Serial.println("connect failed");
        // 실패 시 클라이언트 정리 (풀 슬롯 반환)
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        return false;
    }

    auto* pService = pClient->getService(TEMPIO_SERVICE_UUID);
    if (!pService) {
        Serial.println("service not found");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        return false;
    }

    auto* pDataChar = pService->getCharacteristic(TEMPIO_CHAR_DATA_UUID);
    if (pDataChar && pDataChar->canNotify()) {
        pDataChar->subscribe(true, onDataNotify);
        Serial.println("subscribed to DATA");
    } else {
        // DATA 구독 실패 — 센서값을 받을 수 없으므로 연결 의미 없음
        Serial.println("DATA subscribe failed — disconnecting");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        return false;
    }

    pConfigChar = pService->getCharacteristic(TEMPIO_CHAR_CONFIG_UUID);

    connected = true;
    digitalWrite(LED_PIN, LOW);
    return true;
}

static void sendCommand(const void* data, size_t len) {
    if (!connected || !pConfigChar) return;
    pConfigChar->writeValue(reinterpret_cast<const uint8_t*>(data), len);
}

// ──────────── 공개 API ────────────

void ble_central_init() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    NimBLEDevice::init("tempio-hub");
    NimBLEDevice::setMTU(512);

    auto* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(&scanCb);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->start(0, false);
}

void ble_central_loop() {
    // 끊김 후 재스캔 요청 처리 (콜백에서 위임받음)
    if (doScan) {
        doScan = false;
        NimBLEDevice::getScan()->start(0, false);
    }

    if (doConnect) {
        doConnect = false;
        if (connectToServer()) {
            delay(1000);
            AssignId cmd;
            cmd.node_id = 1; // TODO: 동적 ID 부여
            sendCommand(&cmd, sizeof(cmd));
            Serial.println("<< ASSIGN_ID: 1");
        } else {
            NimBLEDevice::getScan()->start(0, false);
        }
    }

    if (!connected && millis() - lastPrint > 5000) {
        lastPrint = millis();
        Serial.println("scanning...");
    }
}

bool ble_is_connected() {
    return connected;
}
