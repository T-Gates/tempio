#pragma once

// BLE Central — 센서노드/IR노드 스캔·연결·데이터 수신
//
// [완료] TEMPIO UUID로 센서노드 스캔 → 연결 → DATA notify 수신
// [완료] CONFIG 특성으로 ASSIGN_ID 전송
// [TODO] 다중 노드 동시 연결 (현재 1대만)
// [TODO] WiFi + 서버 연동 (Phase 3)
// [TODO] SCD40 CO2 센서 (Phase 4)

void ble_central_init();
void ble_central_loop();
bool ble_is_connected();
