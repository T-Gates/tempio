// 허브 설정값 — 환경에 따라 여기만 수정
#pragma once
#include <cstdint>

// MQTT 브로커 URI (ws:// 또는 wss://)
// 로컬: "ws://192.168.0.7:9001/mqtt"
// 프로덕션: "wss://mqtt.yuumi.wiki/mqtt"
constexpr const char* MQTT_BROKER_URI = "wss://mqtt.yuumi.wiki/mqtt";

// 메인 루프 주기 (ms)
constexpr int LOOP_DELAY_MS = 100;

// BLE 연결 슬롯 수
constexpr int MAX_NODES = 5;
// BLE 연결 대기 큐 크기
constexpr int PENDING_MAX = 8;
// 센서 리포트 링 버퍼 크기
constexpr int REPORT_QUEUE_MAX = 8;

// 노드당 펜딩 명령 큐 크기
constexpr int CMD_PENDING_PER_NODE = 4;
// 펜딩 명령 유효기간 (5분)
constexpr uint32_t CMD_TTL_MS = 300000;

// BLE 스캔 파라미터
constexpr int BLE_SCAN_INTERVAL = 100;
constexpr int BLE_SCAN_WINDOW = 99;

// BLE MTU — NimBLE 연결 시 협상하는 최대 전송 단위
// IR 타이밍 등 큰 패킷을 위해 기본 23에서 확대
constexpr int BLE_MTU = 512;

// BLE 단일 write 최대 크기 — MTU보다 작아야 안전
// ATT 헤더(3바이트) 빼면 실제 페이로드 한도는 MTU-3이지만
// 여유를 두고 500으로 설정
constexpr int BLE_MAX_WRITE_SIZE = 500;

// JSON 직렬화 버퍼 크기
constexpr int REPORT_JSON_BUF_SIZE = 512;  // 센서 리포트 JSON
constexpr int STATUS_JSON_BUF_SIZE = 256;  // 허브 상태 JSON

// 부팅 시 지연 (ms) — 시리얼 모니터 연결 대기용
constexpr int BOOT_DELAY_MS = 3000;
// 부팅 시 Serial.begin 후 대기 타임아웃 (ms)
constexpr uint32_t SERIAL_TIMEOUT_MS = 6000;

// BLE 연결 직후 HUB_READY 전송 전 안정화 대기 (ms)
// NimBLE 내부 MTU 협상이 끝나기를 기다리는 시간
constexpr int POST_CONNECT_DELAY_MS = 200;
