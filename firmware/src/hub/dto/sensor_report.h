// 센서 리포트 DTO — BLE, MQTT, main 간 공유 데이터 구조
#pragma once
#include <stdint.h>
#include <ArduinoJson.h>
#include <WiFi.h>

struct SensorReport {
    char node_id[18];       // MAC 주소 "aa:bb:cc:dd:ee:ff"
    char node_type_str[8];  // "sensor" or "ir"
    float temperature;
    float humidity;
    uint16_t ldr;
    uint16_t battery_mv;
    int8_t ble_rssi;

    // 허브 상태 포함해서 MQTT publish용 JSON 생성
    int toJson(char* buf, size_t bufSize) const {
        JsonDocument doc;
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["uptime_ms"] = millis();

        JsonArray devices = doc["connected_devices"].to<JsonArray>();
        JsonObject dev = devices.add<JsonObject>();
        dev["node_id"] = node_id;
        dev["node_type"] = node_type_str;
        dev["battery_voltage"] = battery_mv / 1000.0f;
        dev["rssi"] = ble_rssi;

        JsonArray readings = doc["sensor_readings"].to<JsonArray>();
        JsonObject reading = readings.add<JsonObject>();
        reading["node_id"] = node_id;
        reading["temperature"] = serialized(String(temperature, 1));
        reading["humidity"] = serialized(String(humidity, 1));
        if (ldr > 0) {
            reading["lux"] = ldr;
        }

        return serializeJson(doc, buf, bufSize);
    }
};
