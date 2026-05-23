#pragma once

// BLE Peripheral — 센서 데이터 광고·전송, 허브 명령 수신
//
// [완료] TEMPIO UUID로 광고 → 허브 연결 수락
// [완료] 연결 시 NodeInfo 전송 + 주기적 SensorData notify
// [완료] CONFIG 특성으로 ASSIGN_ID/SET_INTERVAL/RESET 수신
// [TODO] 실제 센서 연결 — DHT22 → SHT40 (Phase 2)
// [TODO] 딥슬립 (Phase 7)

void ble_peripheral_init();
void ble_peripheral_loop();
bool ble_is_connected();
