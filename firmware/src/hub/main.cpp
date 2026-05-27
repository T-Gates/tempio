// 허브 메인 — setup/loop 오케스트레이터
//
// 실행 순서: WiFi → BLE → MQTT 초기화 후,
// loop에서 시리얼·BLE·MQTT 폴링 + 리포트 발행 + 펜딩 명령 처리
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "ble/ble_central.h"
#include "net/wifi_manager.h"
#include "net/mqtt_handler.h"
#include "cmd/cmd_dispatcher.h"

void setup() {
    delay(3000);
    Serial.begin(115200);
    while (!Serial && millis() < 6000) { delay(10); }

    wifi_init();
    ble_central_init();
    mqtt_init(MQTT_BROKER_URI);
}

void loop() {
    wifi_process_serial();  // 시리얼로 WiFi 설정 변경 감시
    ble_central_loop();     // BLE 스캔·연결·정리
    mqtt_loop();            // WiFi 상태에 따라 MQTT 시작/중지

    // BLE로 들어온 센서 리포트를 MQTT로 서버에 전달
    SensorReport report;
    while (ble_get_pending_report(&report)) {
        // 허브 자체 상태를 리포트에 주입 (노드는 WiFi/힙 정보를 모름)
        report.wifi_rssi  = WiFi.RSSI();
        report.free_heap  = ESP.getFreeHeap();
        report.uptime_ms  = millis();

        char json[512];
        report.toJson(json, sizeof(json));
        mqtt_publish_report(json);
        Serial.printf(">> MQTT published: %s\n", report.node_id);
    }

    // 연결된 노드 중 펜딩 명령이 있으면 전송 시도 (TTL 만료 명령도 여기서 정리)
    flush_all_pending();

    delay(LOOP_DELAY_MS);
}
