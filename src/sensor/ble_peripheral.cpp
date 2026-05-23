#include <Arduino.h>
#include <NimBLEDevice.h>
#include "protocol.h"
#include "ble_peripheral.h"

static constexpr uint8_t LED_PIN = 8;

static NimBLECharacteristic* pDataChar = nullptr;
static NimBLECharacteristic* pConfigChar = nullptr;
// volatile: BLE 태스크와 loop 태스크에서 동시 접근하는 플래그
static volatile bool deviceConnected = false;
static volatile bool sendNodeInfo = false;
static unsigned long lastPrint = 0;
// millis 기반 non-blocking 센서 전송 타이머
static unsigned long lastSend = 0;
static constexpr unsigned long SEND_INTERVAL_MS = 3000;

// ──────────── 콜백 (static 인스턴스로 힙 할당 방지) ────────────

class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
        deviceConnected = true;
        sendNodeInfo = true;
        digitalWrite(LED_PIN, LOW);
        Serial.printf("connected: %s\n", info.getAddress().toString().c_str());
    }

    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
        deviceConnected = false;
        digitalWrite(LED_PIN, HIGH);
        Serial.printf("disconnected (reason=%d)\n", reason);
        NimBLEDevice::startAdvertising();
    }
};

class ConfigCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
        auto val = c->getValue();
        if (val.length() < 1) return;

        auto type = static_cast<MsgType>(val[0]);
        switch (type) {
            case MsgType::ASSIGN_ID: {
                if (val.length() >= sizeof(AssignId)) {
                    auto* cmd = reinterpret_cast<const AssignId*>(val.data());
                    Serial.printf(">> assigned id: %u\n", cmd->node_id);
                    // TODO: NVS에 저장
                }
                break;
            }
            case MsgType::SET_INTERVAL: {
                if (val.length() >= sizeof(SetInterval)) {
                    auto* cmd = reinterpret_cast<const SetInterval*>(val.data());
                    Serial.printf(">> interval: %u sec\n", cmd->interval_sec);
                    // TODO: 딥슬립 주기 변경
                }
                break;
            }
            case MsgType::RESET_NODE: {
                if (val.length() >= sizeof(ResetNode)) {
                    auto* cmd = reinterpret_cast<const ResetNode*>(val.data());
                    Serial.printf(">> reset level: %u\n", cmd->level);
                    // TODO: 레벨별 리셋 처리
                }
                break;
            }
            default:
                Serial.printf(">> unknown config: 0x%02x\n", val[0]);
        }
    }
};

// static 인스턴스 — new 대신 재사용
static ServerCB serverCb;
static ConfigCB configCb;

// ──────────── mock 센서 데이터 ────────────
// TODO: Phase 2에서 DHT22 실제 읽기로 교체

static void sendSensorData() {
    // pDataChar null 체크 (초기화 실패 등 방어)
    if (!pDataChar) return;

    SensorData data;
    data.temp = 24.0f + random(-30, 30) / 10.0f;
    data.humidity = 55.0f + random(-50, 50) / 10.0f;
    data.ldr = random(100, 3000);
    data.battery_mv = 2900 + random(0, 300);

    pDataChar->setValue(reinterpret_cast<uint8_t*>(&data), sizeof(data));
    pDataChar->notify();

    Serial.printf("<< %.1f C  %.1f %%  ldr=%u  bat=%umV\n",
                  data.temp, data.humidity, data.ldr, data.battery_mv);
}

// ──────────── 공개 API ────────────

void ble_peripheral_init() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    NimBLEDevice::init("tempio-sensor");
    NimBLEDevice::setMTU(512);

    auto* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCb);

    auto* pService = pServer->createService(TEMPIO_SERVICE_UUID);

    pDataChar = pService->createCharacteristic(
        TEMPIO_CHAR_DATA_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );

    pConfigChar = pService->createCharacteristic(
        TEMPIO_CHAR_CONFIG_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pConfigChar->setCallbacks(&configCb);

    pServer->start();

    auto* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(TEMPIO_SERVICE_UUID);
    pAdv->start();
}

void ble_peripheral_loop() {
    if (!deviceConnected) {
        if (millis() - lastPrint > 5000) {
            lastPrint = millis();
            Serial.println("advertising...");
        }
        delay(500);
        return;
    }

    if (sendNodeInfo) {
        sendNodeInfo = false;
        if (!pDataChar) return;

        NodeInfo info;
        info.node_type = NodeType::SENSOR;
        info.node_id = 0; // TODO: NVS에서 읽기
        info.battery_mv = 3100;
        info.fw_major = 0;
        info.fw_minor = 1;
        pDataChar->setValue(reinterpret_cast<uint8_t*>(&info), sizeof(info));
        pDataChar->notify();
        Serial.println("<< NodeInfo sent");
        lastSend = millis();
        return;
    }

    // millis() 기반 non-blocking 전송 (delay(3000) 대신)
    if (millis() - lastSend >= SEND_INTERVAL_MS) {
        lastSend = millis();
        sendSensorData();
    }
}

bool ble_is_connected() {
    return deviceConnected;
}
