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

void zbsTx(uint8_t* packetdata, uint8_t len) {
    Serial1.printf("PKT>");
    Serial1.write(len);
    while (len) {
        Serial1.write(*packetdata);
        packetdata++;
        len--;
    }
}

void zbsRxTask(void* parameter) {
    Serial1.begin(230400, SERIAL_8N1, RXD1, TXD1);
    uint8_t RXState = ZBS_RX_WAIT_HEADER;
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
            // Serial.write(lastchar);
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
                        decodePacket(packetp, pktlen);
                        free(packetp);
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
                        } else if(!fsversion){
                            Serial.print("No ZBS/Zigbee FW binary found on SPIFFS, please upload a zigbeebase000X.bin - format binary to enable flashing");
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
