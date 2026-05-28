#include <Arduino.h>
#include <ArduinoJson.h>
#include "hub_command.h"
#include "cmd_dispatcher.h"
#include "../ble/ble_central.h"
#include "../net/mqtt_handler.h"
#include "../config.h"

// 허브 상태를 JSON으로 직렬화해서 MQTT report 토픽에 발행
static void handleHubStatus() {
    char json[STATUS_JSON_BUF_SIZE];
    JsonDocument doc;
    doc["type"] = "hub_status";
    doc["ble_connected"] = bleConnectedCount();
    doc["pending_slots"] = pendingActiveSlots();
    doc["pending_commands"] = pendingTotalCommands();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime_ms"] = millis();
    doc["mqtt_connected"] = mqttIsConnected();
    serializeJson(doc, json, sizeof(json));
    mqttPublishReport(json);
    Serial.println("<< HUB_STATUS published");
}

// 허브 자체 명령 라우터 — cmd.type으로 분기
void handleHubCommand(const MqttCommand& cmd) {
    if (strcmp(cmd.type, "HUB_STATUS") == 0) handleHubStatus();
    else Serial.printf("<< unknown hub cmd: %s\n", cmd.type);
}
