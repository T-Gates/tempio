#include <Arduino.h>
#include "ble_central.h"

// BLE init 시 USB CDC가 잠깐 끊겨서 setup() 메시지는 유실됨
void setup() {
    delay(3000);
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    ble_central_init();
}

void loop() {
    ble_central_loop();
    delay(100);
}
