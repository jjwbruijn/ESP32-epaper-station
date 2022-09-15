#include <Arduino.h>
#include <WiFi.h>
#include <WifiManager.h>

#include "flasher.h"
#include "pendingdata.h"
#include "serial.h"
#include "soc/rtc_wdt.h"
#include "web.h"

void freeHeapTask(void* parameter) {
    while (1) {
        Serial.printf("Free heap=%d\n", ESP.getFreeHeap());
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.print(">\n");
    init_web();
    // WiFiManager wm;
    xTaskCreate(freeHeapTask, "print free heap", 10000, NULL, 2, NULL);
    xTaskCreate(zbsRxTask, "zbsRX Process", 10000, NULL, 2, NULL);
    xTaskCreate(garbageCollection, "pending-data cleanup", 5000, NULL, 1, NULL);
    /*
    wm.setWiFiAutoReconnect(true);
    wm.setConfigPortalTimeout(180);
    bool res = wm.autoConnect("ESP32ZigbeeBase", "password");  // password protected ap
    if (!res) {
        Serial.println("Failed to connect");
        ESP.restart();
    }
    wm.setWiFiAutoReconnect(true);
    */
}

void loop() {
    vTaskDelay(30000 / portTICK_PERIOD_MS);
}