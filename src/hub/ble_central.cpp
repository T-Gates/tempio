#include <Arduino.h>
#include <NimBLEDevice.h>
#include "protocol.h"
#include "ble_central.h"

static constexpr uint8_t LED_PIN = 8;

static NimBLEClient* pClient = nullptr;
static NimBLERemoteCharacteristic* pConfigChar = nullptr;
static bool connected = false;
static bool doConnect = false;
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
            auto* ni = reinterpret_cast<NodeInfo*>(data);
            const char* typeName = (ni->node_type == NodeType::SENSOR) ? "sensor" : "ir";
            Serial.printf(">> NodeInfo: type=%s id=%u bat=%umV fw=%u.%u\n",
                          typeName, ni->node_id, ni->battery_mv,
                          ni->fw_major, ni->fw_minor);
            break;
        }
        case MsgType::SENSOR_DATA: {
            if (len < sizeof(SensorData)) break;
            auto* sd = reinterpret_cast<SensorData*>(data);
            Serial.printf(">> sensor: %.1f C  %.1f %%  ldr=%u  bat=%umV\n",
                          sd->temp, sd->humidity, sd->ldr, sd->battery_mv);
            break;
        }
        default:
            Serial.printf(">> unknown: 0x%02x (%u bytes)\n", data[0], len);
    }
}

// ──────────── 콜백 ────────────

// TEMPIO UUID를 광고하는 디바이스를 찾으면 스캔 중지 → 연결 예약
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
        Serial.printf("disconnected (reason=%d) — restarting scan\n", reason);
        NimBLEDevice::getScan()->start(0, false);
    }
};

// ──────────── 연결·명령 ────────────

// 스캔에서 발견한 노드에 연결 → DATA subscribe + CONFIG 참조 저장
static bool connectToServer() {
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCB());

    if (!pClient->connect(targetAddr)) {
        Serial.println("connect failed");
        return false;
    }

    auto* pService = pClient->getService(TEMPIO_SERVICE_UUID);
    if (!pService) {
        Serial.println("service not found");
        pClient->disconnect();
        return false;
    }

    auto* pDataChar = pService->getCharacteristic(TEMPIO_CHAR_DATA_UUID);
    if (pDataChar && pDataChar->canNotify()) {
        pDataChar->subscribe(true, onDataNotify);
        Serial.println("subscribed to DATA");
    }

    pConfigChar = pService->getCharacteristic(TEMPIO_CHAR_CONFIG_UUID);

    connected = true;
    digitalWrite(LED_PIN, LOW); // LED ON = 연결됨
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
    pScan->setScanCallbacks(new ScanCB());
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->start(0, false);
}

void ble_central_loop() {
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
