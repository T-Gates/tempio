// BLE 스캔 + 연결 관리
#pragma once
#include <NimBLEDevice.h>

void ble_connection_init(NimBLEScan* pScan);
void processNextPending();
