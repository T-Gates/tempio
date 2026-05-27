// BLE 데이터 수신 — notify 콜백으로 센서 데이터를 받아서 리포트 큐에 적재
#include <Arduino.h>
#include <cstring>
#include "ble_internal.h"
#include "ble_data.h"

// ──────────── 센서 리포트 링 버퍼 ────────────
// onDataNotify(NimBLE 태스크)와 ble_get_pending_report(Arduino loop 태스크)가 동시 접근
static SensorReport reportQueue[REPORT_QUEUE_MAX];
static volatile int reportHead = 0;
static volatile int reportTail = 0;
static volatile int reportCount = 0;
static portMUX_TYPE reportMux = portMUX_INITIALIZER_UNLOCKED;

static int resolveSourceSlot(NimBLERemoteCharacteristic* c) {
    auto* svc = c->getRemoteService();
    return svc ? findSlotByClient(svc->getClient()) : -1;
}

static void handleNodeInfo(int slot, const uint8_t* data, size_t len, const char* srcAddr) {
    if (len < sizeof(NodeInfo)) return;
    NodeInfo ni;
    memcpy(&ni, data, sizeof(ni));
    if (slot >= 0) nodes[slot].nodeType = ni.node_type;
    const char* typeName = (ni.node_type == NodeType::SENSOR) ? "sensor" : "ir";
    Serial.printf(">> [%s] NodeInfo: type=%s bat=%umV fw=%u.%u\n",
                  srcAddr, typeName, ni.battery_mv, ni.fw_major, ni.fw_minor);
}

static SensorReport buildReport(int slot, const SensorData& sd, const char* srcAddr) {
    SensorReport rpt;
    strncpy(rpt.node_id, srcAddr, sizeof(rpt.node_id) - 1);
    rpt.node_id[sizeof(rpt.node_id) - 1] = '\0';
    const char* typeStr = (slot >= 0 && nodes[slot].nodeType == NodeType::IR)
        ? "ir" : "sensor";
    strncpy(rpt.node_type_str, typeStr, sizeof(rpt.node_type_str) - 1);
    rpt.node_type_str[sizeof(rpt.node_type_str) - 1] = '\0';
    rpt.temperature = sd.temp;
    rpt.humidity    = sd.humidity;
    rpt.ldr         = sd.ldr;
    rpt.battery_mv  = sd.battery_mv;
    rpt.ble_rssi    = (slot >= 0) ? nodes[slot].client->getRssi() : 0;
    return rpt;
}

static void enqueueReport(const SensorReport& rpt) {
    portENTER_CRITICAL(&reportMux);
    if (reportCount < REPORT_QUEUE_MAX) {
        reportQueue[reportTail] = rpt;
        reportTail = (reportTail + 1) % REPORT_QUEUE_MAX;
        reportCount++;
    }
    portEXIT_CRITICAL(&reportMux);
}

static void handleSensorData(int slot, const uint8_t* data, size_t len, const char* srcAddr) {
    if (len < sizeof(SensorData)) return;
    SensorData sd;
    memcpy(&sd, data, sizeof(sd));
    Serial.printf(">> [%s] sensor: %.1f C  %.1f %%  ldr=%u  bat=%umV\n",
                  srcAddr, sd.temp, sd.humidity, sd.ldr, sd.battery_mv);
    enqueueReport(buildReport(slot, sd, srcAddr));
}

// NimBLE notify 콜백 — 센서노드가 데이터를 push하면 자동 호출
void onDataNotify(NimBLERemoteCharacteristic* c,
                  uint8_t* data, size_t len, bool isNotify) {
    if (len < 1) return;
    int slot = resolveSourceSlot(c);
    // toString()은 임시 std::string — c_str()만 저장하면 댕글링 포인터
    std::string addrStr = (slot >= 0) ? nodes[slot].addr.toString() : "??";
    const char* srcAddr = addrStr.c_str();

    switch (static_cast<MsgType>(data[0])) {
        case MsgType::NODE_INFO:
            handleNodeInfo(slot, data, len, srcAddr);
            break;
        case MsgType::SENSOR_DATA:
            handleSensorData(slot, data, len, srcAddr);
            break;
        default:
            Serial.printf(">> [%s] unknown: 0x%02x (%u bytes)\n",
                          srcAddr, data[0], len);
    }
}

bool ble_get_pending_report(SensorReport* out) {
    portENTER_CRITICAL(&reportMux);
    if (reportCount <= 0) {
        portEXIT_CRITICAL(&reportMux);
        return false;
    }
    *out = reportQueue[reportHead];
    reportHead = (reportHead + 1) % REPORT_QUEUE_MAX;
    reportCount--;
    portEXIT_CRITICAL(&reportMux);
    return true;
}
