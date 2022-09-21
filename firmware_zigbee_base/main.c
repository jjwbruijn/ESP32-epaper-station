#define __packed
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "comms.h"
#include "cpu.h"
#include "printf.h"
#include "proto.h"
#include "radio.h"
#include "timer.h"
#include "wdt.h"

uint16_t version = 0x0011;

static uint8_t __xdata mRxBuf[COMMS_MAX_PACKET_SZ];

uint8_t __xdata mSelfMac[8];
uint8_t __xdata cmdbuffer[4];
uint8_t __xdata packetp[128];
uint8_t __xdata pktindex = 0;
uint8_t __xdata RXState = 0;
uint8_t __xdata pktlen = 0;
uint8_t __xdata serialCRC = 0;
uint8_t __xdata serialCalc = 0;

#define ZBS_RX_WAIT_HEADER 0
#define ZBS_RX_WAIT_PKT_LEN 1
#define ZBS_RX_WAIT_PKT_RX 2
#define ZBS_RX_WAIT_PKT_CRC 3

void processSerial(uint8_t lastchar) {
    // uartTx(lastchar); echo
    switch (RXState) {
        case ZBS_RX_WAIT_HEADER:
            // shift characters in
            for (uint8_t c = 0; c < 3; c++) {
                cmdbuffer[c] = cmdbuffer[c + 1];
            }
            cmdbuffer[3] = lastchar;
            if (strncmp(cmdbuffer, "PKT>", 4) == 0) {
                RXState = ZBS_RX_WAIT_PKT_LEN;
                memset(cmdbuffer, 0x00, 4);
            }
            if (strncmp(cmdbuffer, "MAC?", 4) == 0) {
                pr("MAC>%02X%02X%02X%02X%02X%02X%02X%02X\n", mSelfMac[0], mSelfMac[1], mSelfMac[2], mSelfMac[3], mSelfMac[4], mSelfMac[5], mSelfMac[6], mSelfMac[7]);
            }
            if (strncmp(cmdbuffer, "VER?", 4) == 0) {
                pr("VER>%04X\n", version);
            }
            if (strncmp(cmdbuffer, "RDY?", 4) == 0) {
                pr("RDY>");
            }
            if (strncmp(cmdbuffer, "RSET", 4) == 0) {
                wdtDeviceReset();
            }
            break;
        case ZBS_RX_WAIT_PKT_LEN:
            pktlen = lastchar;
            RXState = ZBS_RX_WAIT_PKT_CRC;
            break;
        case ZBS_RX_WAIT_PKT_CRC:
            serialCRC = lastchar;
            serialCalc = 0;
            pktindex = 0;
            RXState = ZBS_RX_WAIT_PKT_RX;
            break;
        case ZBS_RX_WAIT_PKT_RX:
            packetp[pktindex] = lastchar;
            serialCalc ^= lastchar;
            pktindex++;
            if (pktindex == pktlen) {
                if (serialCalc == serialCRC) {
                    if (commsTxUnencrypted(packetp, pktlen)) {
                        pr("TXC>");
                    } else {
                        pr("TXF!");
                    }
                } else {
                    pr("CRC!\n");
                }
                RXState = ZBS_RX_WAIT_HEADER;
            }
            break;
    }
}

void main(void) {
    clockingAndIntsInit();

    timerInit();
    boardInit();

    P0FUNC = 0b11001111;  // enable uart tx/rx and SPI bus functions

    irqsOn();

    boardInitStage2();

    if (!boardGetOwnMac(mSelfMac)) {
        pr("failed to get MAC. Aborting\n");
        while (1)
            ;
    }

    radioInit();
    radioRxFilterCfg(mSelfMac, 0x10000, PROTO_PAN_ID);

    pr("MAC>%02X%02X%02X%02X%02X%02X%02X%02X\n", mSelfMac[0], mSelfMac[1], mSelfMac[2], mSelfMac[3], mSelfMac[4], mSelfMac[5], mSelfMac[6], mSelfMac[7]);
    pr("VER>%04X\n", version);

    // init the "random" number generation unit
    rndSeed(mSelfMac[0] ^ (uint8_t)timerGetLowBits(), mSelfMac[1]);
    wdtSetResetVal(0xFD0DCF);
    wdtOn();
    if (1) {
        radioSetChannel(RADIO_FIRST_CHANNEL);
        radioSetTxPower(10);
        radioRxEnable(true, true);
        uint8_t __xdata fromMac[8];
        pr("RDY>\n");
        while (1) {
            int8_t ret = commsRxUnencrypted(mRxBuf);
            if (ret > 1) {
                pr("PKT>");
                uartTx(ret);
                for (uint8_t len = 0; len < ret; len++) {
                    uartTx(mRxBuf[len]);
                }
                // wdtSetResetVal(0xFE0DCF);
                //  pr("|%02X%02X", mLastRSSI, mLastLqi); // reading RSSI/LQI seems broken... Needs to be fixed
            }

            while (uartBytesAvail()) {
                processSerial(uartRx());
            }
            wdtSetResetVal(0xFD0DCF);  // watchdog doesn't seem to want to be petted, keeps barking.
        }
    }
}
