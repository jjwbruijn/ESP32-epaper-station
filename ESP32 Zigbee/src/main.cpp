#include <Arduino.h>
#include <WiFi.h>
#include <WifiManager.h>

#include "serial.h"
#include "soc/rtc_wdt.h"

void freeHeapTask(void* parameter) {
    while (1) {
        Serial.printf("Free heap=%d\n", ESP.getFreeHeap());
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}

WiFiManager wm;
void setup() {
    Serial.begin(115200);
    Serial.print(">\n");

    xTaskCreate(freeHeapTask, "print free heap", 10000, NULL, 2, NULL);
    xTaskCreate(zbsRxTask, "zbsRX Process", 10000, NULL, 2, NULL);

    wm.setConfigPortalTimeout(180);
    wm.setWiFiAutoReconnect(true);
    bool res = wm.autoConnect("ESP32ZigbeeBase", "password");  // password protected ap
    if (!res) {
        Serial.println("Failed to connect");
        ESP.restart();
    }
}

void loop() {
    vTaskDelay(30000 / portTICK_PERIOD_MS);
}