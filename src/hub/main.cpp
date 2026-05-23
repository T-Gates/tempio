#include <Arduino.h>
#include <NimBLEDevice.h>

// 테스트: NimBLEDevice::init() 이후에도 USB Serial이 살아있는지
static constexpr uint8_t LED_PIN = 8;

void setup() {
    delay(3000);
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("1. before BLE init");

    NimBLEDevice::init("test");

    Serial.println("2. after BLE init");

    NimBLEDevice::setMTU(512);

    Serial.println("3. after setMTU");

    pinMode(LED_PIN, OUTPUT);
    Serial.println("4. setup done");
}

void loop() {
    Serial.println("5. loop");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(1000);
}
