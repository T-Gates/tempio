#include <Arduino.h>
#include <NimBLEDevice.h>

// 테스트: NimBLE include가 USB Serial을 깨뜨리는지 확인
static constexpr uint8_t LED_PIN = 8;
static int count = 0;

void setup() {
    delay(3000);
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    Serial.println("=== NIMBLE INCLUDE TEST ===");

    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_PIN, LOW);
    Serial.printf("blink %d\n", count++);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
    delay(500);
}
