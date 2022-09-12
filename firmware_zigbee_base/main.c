#define __packed
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adc.h"
#include "asmUtil.h"
#include "board.h"

#include "comms.h"
#include "cpu.h"
#include "printf.h"
#include "proto.h"
#include "radio.h"
#include "sleep.h"
#include "timer.h"
#include "wdt.h"

static const uint64_t __code VERSIONMARKER mVersionRom = 0x0000011100000000ull;

static uint64_t __xdata mVersion;
static uint8_t __xdata mRxBuf[COMMS_MAX_PACKET_SZ];
static uint32_t __idata mTimerWaitStart;
//static struct CommsInfo __xdata mCi;
uint8_t __xdata mSelfMac[8];
uint16_t __xdata battery_voltage = 0;
int8_t __xdata mCurTemperature;

const char __xdata *fwVerString(void) {
    static char __xdata fwVer[32];

    if (!fwVer[0]) {
        spr(fwVer, "FW v%u.%u.%*u.%*u",
            *(((uint8_t __xdata *)&mVersion) + 5),
            *(((uint8_t __xdata *)&mVersion) + 4),
            (uintptr_near_t)(((uint8_t __xdata *)&mVersion) + 2),
            (uintptr_near_t)(((uint8_t __xdata *)&mVersion) + 0));
    }

    return fwVer;
}

const char __xdata *voltString(void) {
    static char __xdata voltStr[32];

    if (!voltStr[0]) {
        spr(voltStr, "%u.%uV",
            (uint16_t)mathPrvDiv32x16(battery_voltage, 1000), mathPrvDiv16x8(mathPrvMod32x16(battery_voltage, 1000), 100));
    }

    return voltStr;
}

void getVolt(void) {
    if (battery_voltage == 0)
        battery_voltage = adcSampleBattery();
}

const char __xdata *macSmallString(void) {
    static char __xdata macStr[32];

    if (!macStr[0]) {
        spr(macStr, "%02X%02X%02X%02X%02X%02X%02X%02X",
            mSelfMac[7],
            mSelfMac[6],
            mSelfMac[5],
            mSelfMac[4],
            mSelfMac[3],
            mSelfMac[2],
            mSelfMac[1],
            mSelfMac[0]);
    }

    return macStr;
}

const char __xdata *macString(void) {
    static char __xdata macStr[28];

    if (!macStr[0])
        spr(macStr, "%M", (uintptr_near_t)mSelfMac);

    return macStr;
}

uint8_t __xdata cmdbuffer[4];
uint8_t __xdata packetp[128];
uint8_t __xdata pktindex = 0;
uint8_t __xdata charindex = 0;
uint8_t __xdata RXState = 0;
uint8_t __xdata pktlen = 0;

#define ZBS_RX_WAIT_HEADER 0
#define ZBS_RX_WAIT_PKT_LEN 1
#define ZBS_RX_WAIT_PKT_RX 2
#define ZBS_RX_WAIT_SEP1 3

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
                charindex = 0;
                memset(cmdbuffer, 0x00, 4);
            }
            if (strncmp(cmdbuffer, "MAC?", 4) == 0) {
                pr("MAC>%02X%02X%02X%02X%02X%02X%02X%02X", mSelfMac[0], mSelfMac[1], mSelfMac[2], mSelfMac[3], mSelfMac[4], mSelfMac[5], mSelfMac[6], mSelfMac[7]);
            }
            break;
        case ZBS_RX_WAIT_PKT_LEN:
            cmdbuffer[charindex] = lastchar;
            charindex++;
            if (charindex == 2) {
                pktlen = (uint8_t)strtoul(cmdbuffer, NULL, 16);
                pr("PKT_LEN=%02X\n", pktlen);
                pktindex = 0;
                charindex = 0;
                RXState = ZBS_RX_WAIT_SEP1;
            }
            break;
        case ZBS_RX_WAIT_SEP1:
            RXState = ZBS_RX_WAIT_PKT_RX;
            break;
        case ZBS_RX_WAIT_PKT_RX:
            cmdbuffer[charindex] = lastchar;
            charindex++;
            if (charindex == 2) {
                charindex = 0;
                uint8_t curbyte = (uint8_t)strtoul(cmdbuffer, NULL, 16);
                packetp[pktindex] = curbyte;
                pktindex++;
                if (pktindex == pktlen) {
                    pr("PKT_SENT");
                    commsTxUnencrypted(packetp, pktlen);
                    RXState = ZBS_RX_WAIT_HEADER;
                }
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

    pr("booted at 0x%04x\n", (uintptr_near_t)&main);

    boardInitStage2();

    if (!boardGetOwnMac(mSelfMac)) {
        pr("failed to get MAC. Aborting\n");
        while (1)
            ;
    }
    // pr("MAC:%ls\n", (uintptr_near_t)macString());

    // mCurTemperature = adcSampleTemperature();
    // pr("temp: %d\n", mCurTemperature);

    // for zbs, this must follow temp reading
    radioInit();

    radioRxFilterCfg(mSelfMac, 0x10000, PROTO_PAN_ID);

    // init the "random" number generation unit
    rndSeed(mSelfMac[0] ^ (uint8_t)timerGetLowBits(), mSelfMac[1]);

    if (1) {
        radioSetChannel(RADIO_FIRST_CHANNEL);
        radioSetTxPower(10);
        radioRxEnable(true, true);
        uint8_t __xdata fromMac[8];
        pr("RDY>\n");
        while (1) {
            int8_t ret = commsRxUnencrypted(mRxBuf);  // commsRx(&mCi, mRxBuf, fromMac);
            if (ret > 1) {
                pr("PKT>%02X|", ret);
                for (uint8_t len = 0; len < ret; len++) {
                    pr("%02X", mRxBuf[len]);
                }
                pr("|%02X%02X", mLastRSSI, mLastLqi);
                pr("<END\n");
            }

            if (uartBytesAvail()) {
                processSerial(uartRx());
            }
        }
        powerPortsDownForSleep();
        sleepForMsec(10000);
        wdtDeviceReset();
    }
}
