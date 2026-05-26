#include <Arduino.h>
#include "config.h"
#include "ble/ble_central.h"
#include "net/wifi_manager.h"
#include "net/mqtt_client.h"

void setup() {
    delay(3000);
    Serial.begin(115200);
    while (!Serial && millis() < 6000) { delay(10); }

    wifi_init();
    ble_central_init();
    mqtt_init(MQTT_BROKER_URI);
}

void loop() {
    wifi_process_serial();
    ble_central_loop();
    mqtt_loop();

    SensorReport report;
    if (ble_get_pending_report(&report)) {
        char json[512];
        report.toJson(json, sizeof(json));
        mqtt_publish_report(json);
        Serial.printf(">> MQTT published: %s\n", report.node_id);
    }

    delay(LOOP_DELAY_MS);
}
