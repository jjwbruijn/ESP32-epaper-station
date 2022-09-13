#include <Arduino.h>
#include <HardwareSerial.h>

#include "zigbee.h"

#define RXD1 13
#define TXD1 12

#define ZBS_RX_WAIT_HEADER 0
#define ZBS_RX_WAIT_PKT_LEN 1
#define ZBS_RX_WAIT_PKT_RX 2
#define ZBS_RX_WAIT_SEP1 3
#define ZBS_RX_WAIT_SEP2 4
#define ZBS_RX_WAIT_MAC 5

void zbsTx(uint8_t* packetdata, uint8_t len) {
    Serial1.printf("PKT>");
    Serial1.write(len);
    while (len) {
        Serial1.write(*packetdata);
        packetdata++;
        len--;
    }
    /*
    Serial1.printf("PKT>%02X|", len);
    while (len) {
        Serial1.printf("%02X", *packetdata);
        packetdata++;
        len--;
    }
    */
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
    Serial1.print("MAC?");
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
                            Serial.printf("Device MAC: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", devicemac[7], devicemac[6], devicemac[5], devicemac[4], devicemac[3], devicemac[2], devicemac[1], devicemac[0]);
                            RXState = ZBS_RX_WAIT_HEADER;
                        }
                    }
                    break;
            }
        }
        if (Serial.available()) {
            Serial1.write(Serial.read());
        }
        yield();
    }
}
