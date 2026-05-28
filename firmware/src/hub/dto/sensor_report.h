// 센서 리포트 DTO — BLE, MQTT, main 간 공유 데이터 구조
#pragma once
#include <stdint.h>
#include <cstdio>
#include <ArduinoJson.h>

struct SensorReport {
    char node_id[18] = {};       // MAC 주소 "aa:bb:cc:dd:ee:ff"
    char node_type_str[8] = {};  // "sensor" or "ir"
    float temperature = 0;
    float humidity = 0;
    uint16_t ldr = 0;
    uint16_t battery_mv = 0;
    int8_t ble_rssi = 0;

    // 허브 상태 — main.cpp에서 직렬화 전에 주입
    int16_t wifi_rssi = 0;
    uint32_t free_heap = 0;
    uint32_t uptime_ms = 0;

    // JSON 직렬화 — snprintf로 소수점 1자리 포맷팅 (String 임시객체 회피)
    int toJson(char* buf, size_t bufSize) const {
        JsonDocument doc;
        doc["node_id"] = node_id;
        doc["node_type"] = node_type_str;

        // String(float, 1) 대신 snprintf → 힙 할당 없이 소수점 1자리
        char tmpTemp[8];
        snprintf(tmpTemp, sizeof(tmpTemp), "%.1f", temperature);
        doc["temperature"] = serialized(tmpTemp);

        char tmpHum[8];
        snprintf(tmpHum, sizeof(tmpHum), "%.1f", humidity);
        doc["humidity"] = serialized(tmpHum);

        if (ldr > 0) doc["lux"] = ldr;
        doc["battery_voltage"] = battery_mv / 1000.0f;
        doc["ble_rssi"] = ble_rssi;
        doc["wifi_rssi"] = wifi_rssi;
        doc["free_heap"] = free_heap;
        doc["uptime_ms"] = uptime_ms;
        return serializeJson(doc, buf, bufSize);
    }
};
