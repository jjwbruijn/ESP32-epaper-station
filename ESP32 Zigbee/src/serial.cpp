#include "serial.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include "flasher.h"
#include "settings.h"
#include "zbs_interface.h"
#include "zigbee.h"

#define ZBS_RX_WAIT_HEADER 0
#define ZBS_RX_WAIT_PKT_LEN 1
#define ZBS_RX_WAIT_PKT_RX 2
#define ZBS_RX_WAIT_SEP1 3
#define ZBS_RX_WAIT_SEP2 4
#define ZBS_RX_WAIT_MAC 5
#define ZBS_RX_WAIT_VER 6

#define QUEUESIZE 50
QueueHandle_t outQueue;
QueueHandle_t inQueue;

#define STATE_TX_COMPLETE 0
#define STATE_TX_CRC_ERROR 1
#define STATE_TX_ERROR 2
#define STATE_TX_WAIT 3
#define STATE_TX_RDY 4

volatile uint8_t txcState = STATE_TX_COMPLETE;

uint8_t RXState = ZBS_RX_WAIT_HEADER;

void zbsTx(uint8_t* packetdata, uint8_t len, uint8_t retry) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= packetdata[i];
    }

    Serial1.print("RDY?");
    uint8_t c = 0;
    while (txcState != STATE_TX_RDY) {
        c++;
        if (c % 5 == 0) Serial1.print("RDY?");
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    Serial1.printf("PKT>");
    Serial1.write(len);
    Serial1.write(crc);
    while (len) {
        Serial1.write(*packetdata);
        ++packetdata;
        --len;
    }

    txcState = STATE_TX_WAIT;
    unsigned long startt = millis();
    while (startt + 15 > millis()) {
        vTaskDelay(2 / portTICK_PERIOD_MS);
        Serial1.write(0x00);
        switch (txcState) {
            case STATE_TX_COMPLETE:
                return;
            case STATE_TX_CRC_ERROR:
                Serial.print("CRC ERROR :(\n");
                zbsTx(packetdata, len, retry);
                return;
            case STATE_TX_ERROR:
                Serial.print("TX ERROR...\n");
                zbsTx(packetdata, len, retry);
                return;
            case STATE_TX_WAIT:
                break;
        }
    }
    if (retry--) {
        zbsTx(packetdata, len, retry);
    } else {
        Serial.printf("\nXMIT TIMED OUT!\n", txcState, RXState);
    }
}

void zbsRxTask(void* parameter) {
    Serial1.begin(230400, SERIAL_8N1, RXD1, TXD1);

    inQueue = xQueueCreate(QUEUESIZE, sizeof(struct serialFrame*));
    outQueue = xQueueCreate(QUEUESIZE, sizeof(struct serialFrame*));

    xTaskCreate(zigbeeDecodeTask, "zigbee decode process", 10000, NULL, 2, NULL);

    char cmdbuffer[4] = {0};
    uint8_t* packetp = nullptr;
    uint8_t pktlen = 0;
    uint8_t pktindex = 0;
    char lastchar = 0;
    uint8_t charindex = 0;
    simplePowerOn();
    bool waitingForVersion = true;
    uint16_t version;

    while (1) {
        if (Serial1.available()) {
            lastchar = Serial1.read();
            //Serial.write(lastchar);
            switch (RXState) {
                case ZBS_RX_WAIT_HEADER:
                    // shift characters in
                    for (uint8_t c = 0; c < 3; c++) {
                        cmdbuffer[c] = cmdbuffer[c + 1];
                    }
                    cmdbuffer[3] = lastchar;
                    if (strncmp(cmdbuffer, "PKT>", 4) == 0) {
                        RXState = ZBS_RX_WAIT_PKT_LEN;
                        charindex = 0;
                        memset(cmdbuffer, 0x00, 4);
                    }
                    if (strncmp(cmdbuffer, "MAC>", 4) == 0) {
                        pktindex = 0;
                        RXState = ZBS_RX_WAIT_MAC;
                        charindex = 0;
                        memset(cmdbuffer, 0x00, 4);
                    }
                    if ((strncmp(cmdbuffer, "VER>", 4) == 0) && waitingForVersion) {
                        waitingForVersion = false;
                        pktindex = 0;
                        RXState = ZBS_RX_WAIT_VER;
                        charindex = 0;
                        memset(cmdbuffer, 0x00, 4);
                    }
                    if (strncmp(cmdbuffer, "TXC>", 4) == 0) {
                        txcState = STATE_TX_COMPLETE;
                    }
                    if (strncmp(cmdbuffer, "TXF!", 4) == 0) {
                        txcState = STATE_TX_ERROR;
                    }
                    if (strncmp(cmdbuffer, "CRC!", 4) == 0) {
                        txcState = STATE_TX_CRC_ERROR;
                    }
                    if (strncmp(cmdbuffer, "RDY>", 4) == 0) {
                        txcState = STATE_TX_RDY;
                    }
                    break;
                case ZBS_RX_WAIT_PKT_LEN:
                    pktlen = lastchar;
                    packetp = (uint8_t*)calloc(pktlen, 1);
                    pktindex = 0;
                    charindex = 0;
                    RXState = ZBS_RX_WAIT_PKT_RX;
                    break;
                case ZBS_RX_WAIT_SEP1:
                    RXState = ZBS_RX_WAIT_PKT_RX;
                    break;
                case ZBS_RX_WAIT_PKT_RX:
                    packetp[pktindex] = lastchar;
                    pktindex++;
                    if (pktindex == pktlen) {
                        struct serialFrame* frame = new struct serialFrame;
                        frame->len = pktlen;
                        frame->data = packetp;
                        BaseType_t queuestatus = xQueueSend(inQueue, &frame, 0);
                        if (queuestatus == pdFALSE) {
                            Serial.print("input queue is full...\n");
                            free(frame->data);
                            delete frame;
                        }
                        RXState = ZBS_RX_WAIT_HEADER;
                    }
                    break;
                case ZBS_RX_WAIT_MAC:
                    cmdbuffer[charindex] = lastchar;
                    charindex++;
                    if (charindex == 2) {
                        charindex = 0;
                        uint8_t curbyte = (uint8_t)strtoul(cmdbuffer, NULL, 16);
                        devicemac[pktindex] = curbyte;
                        pktindex++;
                        if (pktindex == 8) {
                            Serial.printf("ZBS/Zigbee MAC: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", devicemac[7], devicemac[6], devicemac[5], devicemac[4], devicemac[3], devicemac[2], devicemac[1], devicemac[0]);
                            RXState = ZBS_RX_WAIT_HEADER;
                        }
                    }
                    break;
                case ZBS_RX_WAIT_VER:
                    cmdbuffer[charindex] = lastchar;
                    charindex++;
                    if (charindex == 4) {
                        charindex = 0;
                        version = (uint16_t)strtoul(cmdbuffer, NULL, 16);
                        uint16_t fsversion;
                        lookupFirmwareFile(fsversion);
                        if ((fsversion) && (version != fsversion)) {
                            Serial.printf("ZBS/Zigbee FW version: %04X, version on SPIFFS: %04X\n", version, fsversion);
                            Serial.printf("Performing flash update in about 30 seconds");
                            vTaskDelay(30000 / portTICK_PERIOD_MS);
                            performDeviceFlash();
                        } else if (!fsversion) {
                            Serial.println("No ZBS/Zigbee FW binary found on SPIFFS, please upload a zigbeebase000X.bin - format binary to enable flashing");
                        } else {
                            Serial.printf("ZBS/Zigbee FW version: %04X\n", version);
                        }
                        RXState = ZBS_RX_WAIT_HEADER;
                    }
                    break;
            }
        }
        if (Serial.available()) {
            Serial1.write(Serial.read());
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
        if (waitingForVersion) {
            if (millis() > 30000) {
                waitingForVersion = false;
                Serial.printf("We've been waiting for communication from the tag, but got nothing. This is expected if this tag hasn't been flashed yet. We'll try to flash it.\n");
                performDeviceFlash();
            }
        }
    }
}
