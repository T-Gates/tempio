// 센서노드 — 딥슬립 one-shot 패턴
//
// wake → BLE 초기화 → 허브 연결 대기 → HUB_READY 대기 →
// NODE_INFO 전송 (id=0이면 ASSIGN_ID 수신 대기) →
// 센서 데이터 전송 → 추가 명령 대기 → disconnect → 딥슬립

#include <Arduino.h>
#include <esp_sleep.h>
#include "ble_peripheral.h"

void setup() {
    Serial.begin(115200);
    delay(100);

    ble_peripheral_init();

    if (ble_wait_connect(5000)) {
        if (ble_wait_hub_ready(3000)) {
            ble_send_node_info();

            // 신규 노드(id=0)면 허브가 ASSIGN_ID를 보내줄 때까지 대기
            if (ble_get_node_id() == 0) {
                ble_wait_config(3000);
            }

            delay(100);
            ble_send_sensor_data();

            // 추가 명령(SET_INTERVAL 등) 수신 여유
            delay(500);
        }
    } else {
        Serial.println("no hub, sleeping");
    }

    ble_disconnect();

    uint16_t interval = ble_get_sleep_interval();
    Serial.printf("sleep %u sec\n", interval);
    esp_sleep_enable_timer_wakeup((uint64_t)interval * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {}
