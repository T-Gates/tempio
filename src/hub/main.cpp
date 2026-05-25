#include <Arduino.h>
#include "ble_central.h"

void setup() {
    delay(3000);
    Serial.begin(115200);
    while (!Serial && millis() < 6000) { delay(10); }
    ble_central_init();
}

void loop() {
    ble_central_loop();
    delay(100);
}
