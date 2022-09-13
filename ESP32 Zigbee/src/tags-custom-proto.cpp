#include "tags-custom-proto.h"
#include <Arduino.h>
#include "zigbee.h"  // <<
#include "testimage.h"


void sendAssociate(uint8_t* dst) {
    Serial.printf("Sending associate packet...\n");
    uint8_t* data = (uint8_t*)calloc(sizeof(struct AssocInfo) + 1, 1);
    struct AssocInfo* assoc = (struct AssocInfo*)(data + 1);
    *data = PKT_ASSOC_RESP;
    memset(assoc, 0, sizeof(struct AssocInfo));
    assoc->checkinDelay = 900000;
    assoc->retryDelay = 1000;
    assoc->failedCheckinsTillBlank = 2;
    assoc->failedCheckinsTillDissoc = 0;
    memcpy(assoc->newKey, preshared_key, 16);
    encodePacket(dst, data, sizeof(struct AssocInfo) + 1);
    free(data);
}

void sendPending(uint8_t* dst) {
    Serial.printf("Sending pending...\n");
    uint8_t* data = (uint8_t*)calloc(sizeof(struct PendingInfo) + 1, 1);
    struct PendingInfo* pi = (struct PendingInfo*)(data + 1);
    *data = PKT_CHECKOUT;
    memset(pi, 0, sizeof(struct PendingInfo));
    pi->imgUpdateVer = 7138543897416935771; 
    // this determines wheter or not a tag will update! Note that tag use the highest 'ver' to see if an update is needed, since this 
    // normally derived from file timestamp. We don't have that on the ESP32 just yet, so enter a pretty high number here and you should be good.
    // If a tag only checks in without requesting an update, this is where your problem lies.
    pi->imgUpdateSize = sizeof(ap);
    encodePacket(dst, data, sizeof(struct PendingInfo) + 1);
    free(data);
}

void sendChunk(uint8_t* dst, struct ChunkReqInfo* chunkreq) {
    uint8_t* data = (uint8_t*)calloc(sizeof(struct ChunkInfo) + chunkreq->len + 1, 1);
    struct ChunkInfo* chunk = (struct ChunkInfo*)(data + 1);
    *data = PKT_CHUNK_RESP;
    memset(chunk, 0, sizeof(struct PendingInfo));
    memcpy(chunk->data, &ap[chunkreq->offset], chunkreq->len);
    chunk->offset = chunkreq->offset;
    encodePacket(dst, data, sizeof(struct ChunkInfo) + chunkreq->len + 1);
    free(data);
}

void processCheckin(uint8_t* src, struct CheckinInfo* ci) {
    Serial.printf("Check-in: Battery = %d\n", ci->state.batteryMv);
    sendPending(src);
}

void processAssociateReq(uint8_t* src, struct TagInfo* taginfo) {
    Serial.printf("Tag %dx%d (%dx%dmm)\n", taginfo->screenPixWidth, taginfo->screenPixHeight, taginfo->screenMmWidth, taginfo->screenMmHeight);
    sendAssociate(src);
}

void processChunkReq(uint8_t* src, struct ChunkReqInfo* chunkreq) {
    Serial.printf("Chunk req offset: %d Len: %d OS_Upt: %d\n", chunkreq->offset, chunkreq->len, chunkreq->osUpdatePlz);
    sendChunk(src, chunkreq);
}

void parsePacket(uint8_t* src, void* data, uint8_t len) {
    if (((uint8_t*)data)[0] == PKT_ASSOC_REQ) {
        Serial.printf("Association request received\n");
        struct TagInfo* taginfo = (struct TagInfo*)(data + 1);
        processAssociateReq(src, taginfo);
    } else if (((uint8_t*)data)[0] == PKT_ASSOC_RESP) {  // not relevant for a base
        Serial.printf("Association response. Not doing anything with that...\n");
        struct AssocInfo* associnfo = (struct AssocInfo*)(data + 1);
    } else if (((uint8_t*)data)[0] == PKT_CHECKIN) {
        struct CheckinInfo* ci = (struct CheckinInfo*)(data + 1);
        processCheckin(src, ci);
    } else if (((uint8_t*)data)[0] == PKT_CHUNK_REQ) {
        struct ChunkReqInfo* chunkreq = (struct ChunkReqInfo*)(data + 1);
        processChunkReq(src, chunkreq);
    } else {
        Serial.printf("Received a frame of type %02X, currently unimplemented :<\n", ((uint8_t*)data)[0]);
    }
}
