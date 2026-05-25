#pragma once
#include <stdint.h>

// BLE Peripheral 초기화.
// NimBLE 시작 + Server/Service/Characteristic 생성 + 광고 시작.
// NVS에서 node_id, sleep_interval 복원.
void ble_peripheral_init();

// 허브 연결을 기다린다. timeout_ms 안에 연결되면 true.
bool ble_wait_connect(uint32_t timeout_ms);

// 허브가 HUB_READY를 보낼 때까지 기다린다.
// 이게 오면 허브의 subscribe가 완료됐다는 뜻 → 데이터 전송 안전.
bool ble_wait_hub_ready(uint32_t timeout_ms);

// 허브가 ASSIGN_ID 등 후속 명령을 보낼 때까지 기다린다.
// 신규 노드(id=0)가 NODE_INFO를 보낸 뒤 ID 부여를 기다릴 때 사용.
bool ble_wait_config(uint32_t timeout_ms);

// NodeInfo(자기 소개) 전송.
void ble_send_node_info();

// 센서 데이터 전송.
void ble_send_sensor_data();

// 명시적 BLE 연결 해제 + NVS 닫기 + BLE 스택 종료.
void ble_disconnect();

// 현재 연결 여부.
bool ble_is_connected();

// 현재 노드 ID. 0이면 미할당 (신규 노드).
uint8_t ble_get_node_id();

// 딥슬립 주기 (초). 10~3600 범위. 허브가 SET_INTERVAL로 변경 가능.
uint16_t ble_get_sleep_interval();
