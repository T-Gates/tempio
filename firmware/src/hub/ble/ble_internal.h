// BLE 모듈 내부 공유 상태 — ble/ 폴더 안에서만 include
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "protocol.h"
#include "../config.h"

// C3=GPIO8(active-low), ESP32=GPIO2(active-high)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
static constexpr uint8_t LED_PIN = 8;
static constexpr bool LED_ON = LOW;
#else
static constexpr uint8_t LED_PIN = 2;
static constexpr bool LED_ON = HIGH;
#endif

struct ConnectedNode {
    NimBLEClient* client = nullptr;
    NimBLERemoteCharacteristic* configChar = nullptr;
    NimBLEAddress addr;
    NodeType nodeType = NodeType::SENSOR;
    bool     used     = false;
};

// ble_central.cpp에서 정의, 다른 ble_*.cpp에서 extern 접근
extern ConnectedNode nodes[MAX_NODES];
extern volatile bool doScan;

// 슬롯 검색 헬퍼 (ble_central.cpp에서 구현)
bool isAlreadyConnected(const NimBLEAddress& addr);
int findEmptySlot();
int findSlotByClient(const NimBLEClient* c);
int findSlotByAddr(const NimBLEAddress& addr);
int activeCount();
void cleanupDisconnected();
NimBLEClient* findReusableClient();
