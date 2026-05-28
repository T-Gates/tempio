// MQTT 명령 DTO — 서버 → 허브 명령 하나를 표현하는 데이터 구조
//
// SensorReport와 마찬가지로 dto/ 폴더에 배치.
// 여러 모듈(mqtt_handler, cmd_dispatcher, pending_pool, hub_command)에서 공유.
#pragma once
#include <stdint.h>

// 서버 → 허브로 오는 명령 하나. MQTT JSON에서 파싱됨.
struct MqttCommand {
    char target[18];    // 대상 노드 MAC "aa:bb:cc:dd:ee:ff"
    char type[16];      // 명령 타입: "SET_INTERVAL", "IR_TIMING", "RESET_NODE"
    char payload[256];  // JSON payload 문자열 (타입별 파라미터)
    uint16_t cmd_id;    // 서버가 부여한 명령 ID. ACK 매칭용.
    uint32_t queued_at; // 큐 진입 시각 (millis). 펜딩 풀에서 TTL 판단용.
};
