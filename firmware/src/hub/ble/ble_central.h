// BLE Central 공개 API — 외부 모듈(main.cpp 등)에서 사용
#pragma once
#include <stdint.h>
#include "../dto/sensor_report.h"
#include "ble_data.h"

void ble_central_init();
void ble_central_loop();
int ble_connected_count();
bool ble_send_to_node(const char* addrStr, const void* data, size_t len);
// ble_get_pending_report()는 ble_data.h에서 선언
