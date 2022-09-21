#include "zigbee.h"

#include <Arduino.h>

#include "mbedtls/aes.h"
#include "mbedtls/ccm.h"
#include "serial.h"  // <<
#include "settings.h"
#include "tags-custom-proto.h"  // >>
#if DEBUG_LEVEL > 0
#include "mbedtls/debug.h"
#endif
#include "mbedtls/platform.h"

struct ccm_data {
    uint8_t nonce[13];
    uint8_t tag[4];
    uint8_t* hdr;
    uint8_t hdrsize;
    uint8_t* encrypted;
    uint8_t datalen;
    uint8_t* data;
};

uint32_t preshared_key[4] = PROTO_PRESHARED_KEY;
uint8_t sequence = 0;
uint8_t devicemac[8] = {0};

void dumpHex(const void* p, const uint16_t len) {
    const uint8_t* data = (const uint8_t*)p;
    for (uint16_t c = 0; c < len; c++) {
        if (c % 16 == 0) {
            Serial.printf("\n%02X: ", c);
        }
        Serial.printf("%02X ", data[c]);
    }
    Serial.printf("\n");
}

void dumpFcs(struct MacFcs* fcs) {
    Serial.printf("Struct: %04X ------\n", *((uint16_t*)fcs));
    Serial.printf("frametype=%d secure=%d framepending=%d\n", fcs->frameType, fcs->secure, fcs->framePending);
    Serial.printf("ackReqd=%d, panIdcompressed=%d, rfu1=%d, rfu2=%d\n", fcs->ackReqd, fcs->panIdCompressed, fcs->rfu1, fcs->rfu2);
    Serial.printf("destaddrtype=%d, framever=%d, srcaddrtype=%d\n", fcs->destAddrType, fcs->frameVer, fcs->srcAddrType);
    Serial.printf("\n");
}

void dumpFrame(void* f, enum macframetype type) {
    // dumpFcs(&(frame->fcs));
    struct MacFrameBcast* bframe = (struct MacFrameBcast*)f;
    struct MacFrameFromMaster* mframe = (struct MacFrameFromMaster*)f;
    struct MacFrameNormal* frame = (struct MacFrameNormal*)f;
    switch (type) {
        case MACFRAME_TYPE_BROADCAST:
            Serial.printf("seq=0x%02X destPan=0x%04X destAddr=0x%04X\n", bframe->seq, bframe->dstPan, bframe->dstAddr);
            Serial.printf("srcPan=0x%04X, src=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", bframe->srcPan, bframe->src[7], bframe->src[6], bframe->src[5], bframe->src[4], bframe->src[3], bframe->src[2], bframe->src[1], bframe->src[0]);
            break;
        case MACFRAME_TYPE_MASTER:
            Serial.printf("seq=0x%02X pan=0x%04X\n", mframe->seq, mframe->pan);
            Serial.printf("dst=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X from=%04X\n", mframe->dst[7], mframe->dst[6], mframe->dst[5], mframe->dst[4], mframe->dst[3], mframe->dst[2], mframe->dst[1], mframe->dst[0], mframe->from);
            break;
        case MACFRAME_TYPE_NORMAL:
            Serial.printf("seq=0x%02X pan=0x%04X\n", mframe->seq, mframe->pan);
            Serial.printf("dst=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X \n", frame->dst[7], frame->dst[6], frame->dst[5], frame->dst[4], frame->dst[3], frame->dst[2], frame->dst[1], frame->dst[0]);
            Serial.printf("src=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X \n", frame->src[7], frame->src[6], frame->src[5], frame->src[4], frame->src[3], frame->src[2], frame->src[1], frame->src[0]);
            break;
    }
}

void dumpCcm(struct ccm_data* ccm) {
    Serial.printf("data=");
    dumpHex(ccm->data, ccm->datalen);
    Serial.printf("encrypted=");
    dumpHex(ccm->encrypted, ccm->datalen);
    Serial.printf("nonce=");
    dumpHex(ccm->nonce, 13);
    Serial.printf("hdr=");
    dumpHex(ccm->hdr, ccm->hdrsize);
    Serial.printf("tag=");
    dumpHex(ccm->tag, 4);
}
bool decrypt(struct ccm_data* ccm_data) {
    int ret = 0;
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, (unsigned char*)preshared_key, 128);
    ret = mbedtls_ccm_auth_decrypt(&ccm, ccm_data->datalen, ccm_data->nonce, 13, ccm_data->hdr, ccm_data->hdrsize, ccm_data->encrypted, ccm_data->data, ccm_data->tag, 4);
    mbedtls_ccm_free(&ccm);
    if (ret != 0) {
        mbedtls_printf("auth_decrypt testing! returned -0x%04X\r\n", -ret);
        return false;
    }
    return true;
}

bool encrypt(struct ccm_data* ccm_data) {
    int ret = 0;
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, (unsigned char*)preshared_key, 128);
    ret = mbedtls_ccm_encrypt_and_tag(&ccm, ccm_data->datalen, ccm_data->nonce, 13, ccm_data->hdr, ccm_data->hdrsize, ccm_data->data, ccm_data->encrypted, ccm_data->tag, 4);
    mbedtls_ccm_free(&ccm);
    if (ret != 0) {
        mbedtls_printf("encrypt returned -0x%04X\r\n", -ret);
        return false;
    }
    return true;
}

void zigbeeDecodeTask(void* parameter) {
    while (true) {
        struct serialFrame* f;
        BaseType_t rx = xQueueReceive(inQueue, &f, portMAX_DELAY);
        if (rx == pdTRUE) {
            decodePacket(f->data, f->len);
            free(f->data);
            delete f;
        }
        vTaskDelay(1/portTICK_PERIOD_MS);
    }
}

void decodePacket(const uint8_t* p, uint8_t len) {
    // dumpHex(p, len);

    struct ccm_data ccm_data;
    uint8_t* src = nullptr;

    // prep a part of the nonce data, what we know before we determine frame type and source address
    memcpy(&ccm_data.nonce, (void*)(p + (len - 4)), 4);
    ccm_data.nonce[12] = 0x00;

    // decode frame data to determine macframe type
    struct MacFcs* fcs = (struct MacFcs*)p;

    if ((fcs->panIdCompressed == 0) && (fcs->destAddrType == 2) && (fcs->frameType == 1) && (fcs->srcAddrType == 3)) {  // broadcast grame
        struct MacFrameBcast* broadcastpacket = (struct MacFrameBcast*)p;
        ccm_data.datalen = len - sizeof(struct MacFrameBcast) - 8;
        ccm_data.hdrsize = sizeof(struct MacFrameBcast);
        src = broadcastpacket->src;
        memcpy(&(ccm_data.nonce[0]) + 4, &(broadcastpacket->src[0]), 8);
    } else if ((fcs->panIdCompressed == 1) && (fcs->destAddrType == 3) && (fcs->frameType == 1) && (fcs->srcAddrType == 3)) {  // normal frame
        struct MacFrameNormal* frame = (struct MacFrameNormal*)p;
        ccm_data.datalen = len - sizeof(struct MacFrameNormal) - 8;
        ccm_data.hdrsize = sizeof(struct MacFrameNormal);
        src = frame->src;
        memcpy(&(ccm_data.nonce[0]) + 4, &(frame->src[0]), 8);
    } else {
        // master frame.... haven't seen those just yet!
        struct MacFrameFromMaster* masterframe = (struct MacFrameFromMaster*)p;
        dumpFrame((void*)masterframe, MACFRAME_TYPE_NORMAL);
        Serial.printf("UNKNOWN FRAME TYPE! MAYBE MASTER?\n");
    }

    // prepare memory for encrypted data, output data, and header
    ccm_data.encrypted = (uint8_t*)calloc(ccm_data.datalen, 1);
    ccm_data.data = (uint8_t*)calloc(ccm_data.datalen, 1);
    ccm_data.hdr = (uint8_t*)calloc(ccm_data.hdrsize, 1);
    memcpy(ccm_data.encrypted, (void*)(p + ccm_data.hdrsize), ccm_data.datalen);
    memcpy(ccm_data.tag, (void*)(p + (len - 8)), 4);
    memcpy(ccm_data.hdr, p, ccm_data.hdrsize);

    const bool isValid = decrypt(&ccm_data);
    // Serial.print("output:\n");
    // dumpHex(ccm_data.data, ccm_data.datalen);

    if (isValid) {
        parsePacket(src, ccm_data.data, ccm_data.datalen);
    }

    // free the memory for the encryption related data;
    free(ccm_data.encrypted);
    free(ccm_data.data);
    free(ccm_data.hdr);
}

void encodePacket(const uint8_t* dst, uint8_t* data, const uint8_t len) {
    if ((dst[0] == 0xFF) && (dst[1] == 0xFF)) {
        // broadcast packet (unimplemented)
    } else {
        // Serial.printf("DATA=");
        // dumpHex(data, len);
        const uint8_t totallen = sizeof(struct MacFrameNormal) + len + 4 + 4;  //(tag + nonce);
        struct MacFrameNormal hdr;
        memset(&hdr, 0, sizeof(struct MacFrameNormal));
        memcpy(hdr.dst, dst, 8);
        memcpy(hdr.src, devicemac, 8);
        hdr.pan = PROTO_PAN_ID;
        hdr.seq = sequence++;
        hdr.fcs.frameType = 1;
        hdr.fcs.panIdCompressed = 1;
        hdr.fcs.destAddrType = 3;
        hdr.fcs.srcAddrType = 3;
        //hdr.fcs.ackReqd = 1;
        struct ccm_data ccm;
        memset(&ccm, 0, sizeof(struct ccm_data));
        uint8_t* output = (uint8_t*)calloc(totallen, 1);
        ccm.data = data;
        ccm.datalen = len;
        ccm.encrypted = output + sizeof(struct MacFrameNormal);
        ccm.hdr = (uint8_t*)&hdr;
        ccm.hdrsize = sizeof(struct MacFrameNormal);
        const uint32_t timestamp = millis();
        memcpy(output + totallen - 4, &timestamp, 4);
        memcpy(ccm.nonce, &timestamp, 4);
        memcpy(ccm.nonce + 4, devicemac, 8);
        memcpy(output, &hdr, sizeof(struct MacFrameNormal));
        ccm.hdr = (uint8_t*)&hdr;
        encrypt(&ccm);
        // dumpCcm(&ccm);
        memcpy(output + totallen - 8, ccm.tag, 4);
        zbsTx(output, totallen, ZIGBEE_ATTEMPTS);
        // Serial.printf("total=");
        // dumpHex(output, totallen);
        free(output);
    }
}