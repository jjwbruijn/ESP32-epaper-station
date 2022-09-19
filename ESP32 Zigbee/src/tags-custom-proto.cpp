#include "tags-custom-proto.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "http.h"  // <<
#include "pendingdata.h"
#include "settings.h"
#include "standalone_filehandling.h"
#include "testimage.h"
#include "time.h"
#include "zigbee.h"  // <<

#define CHECKIN_TEMP_OFFSET 0x7f

unsigned long getTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return (0);
    }
    time(&now);
    return now;
}

#ifdef NETWORK_MODE
void sendAssociateReply(uint8_t *dst, String associatereplydata) {
    Serial.printf("Sending associate reply...\n");
    uint8_t *data = (uint8_t *)calloc(sizeof(struct AssocInfo) + 1, 1);
    struct AssocInfo *assoc = (struct AssocInfo *)(data + 1);
    *data = PKT_ASSOC_RESP;
    memset(assoc, 0, sizeof(struct AssocInfo));
    memcpy(assoc->newKey, preshared_key, 16);

    // parse json data from the server
    StaticJsonDocument<512> assocreply;
    DeserializationError error = deserializeJson(assocreply, associatereplydata);
    assoc->checkinDelay = assocreply["checkinDelay"];
    assoc->retryDelay = assocreply["retryDelay"];
    assoc->failedCheckinsTillBlank = assocreply["failedCheckinsTillBlank"];
    assoc->failedCheckinsTillDissoc = assocreply["failedCheckinsTillDissoc"];
    assoc->rfu[0] = assocreply["rfu"];  // broken

    encodePacket(dst, data, sizeof(struct AssocInfo) + 1);
    free(data);
}

void sendPending(uint8_t *dst, String pendingdatastr) {
    Serial.printf("Sending check-in reply\n");

    // reserve space for pending data
    uint8_t *data = (uint8_t *)calloc(sizeof(struct PendingInfo) + 1, 1);
    struct PendingInfo *pi = (struct PendingInfo *)(data + 1);
    *data = PKT_CHECKOUT;
    memset(pi, 0, sizeof(struct PendingInfo));

    // parse json data from the server
    StaticJsonDocument<512> pending;
    DeserializationError error = deserializeJson(pending, pendingdatastr);
    pi->imgUpdateSize = pending["imgLen"];
    pi->imgUpdateVer = int(pending["imgMTime"]) << 32 | int(pending["imgCTime"]);
    pi->osUpdateSize = pending["osLen"];
    pi->osUpdateVer = int(pending["osMTime"]) << 32 | int(pending["osCTime"]);

    // send pending-data to the tag
    encodePacket(dst, data, sizeof(struct PendingInfo) + 1);

    // save information about pending data to array
    pendingdata *pendinginfo = pendingdata::findByVer(pi->imgUpdateVer);
    if (pendinginfo == nullptr) {
        String file = pending["imgFile"];
        String md5 = pending["imgMd5"];
        pendinginfo = new pendingdata;
        memcpy(pendinginfo->dst, dst, 8);
        pendinginfo->timeout = PENDING_TIMEOUT;
        pendinginfo->filename = file;
        pendinginfo->ver = pi->imgUpdateVer;
        pendinginfo->md5 = md5;
        pendinginfo->len = pi->imgUpdateSize;
        pendinginfo->data = nullptr;
        pendingfiles.push_back(pendinginfo);
    } else {
        pendinginfo->timeout = PENDING_TIMEOUT;
    }
    free(data);
    portYIELD();
}

void sendChunk(uint8_t *dst, struct ChunkReqInfo *chunkreq) {
    uint8_t *data = (uint8_t *)calloc(sizeof(struct ChunkInfo) + chunkreq->len + 1, 1);
    struct ChunkInfo *chunk = (struct ChunkInfo *)(data + 1);
    *data = PKT_CHUNK_RESP;
    memset(chunk, 0, sizeof(struct PendingInfo));
    chunk->offset = chunkreq->offset;

    // find info about this chunk version
    pendingdata *pending = pendingdata::findByVer(chunkreq->versionRequested);
    if (pending == nullptr) {
        Serial.printf("Looking for chunk info with version %d but I couldn't find it :(\n", chunkreq->versionRequested);
    } else {
        if (!pending->data) {
            // data not available, get it now.
            Serial.printf("Data not found, asking the webserver");
            pending->len = getDataFromHTTP(&(pending->data), pending->filename);
            Serial.printf(" - got %d bytes from server\n", pending->len);
        }
        Serial.printf("Serving offset %d from buffer - %d%%\n", chunkreq->offset, (100 * chunkreq->offset + 1 + chunkreq->len) / pending->len);
        memcpy(chunk->data, &(pending->data[chunkreq->offset]), chunkreq->len);
    }
    encodePacket(dst, data, sizeof(struct ChunkInfo) + chunkreq->len + 1);
    free(data);
}

void processChunkReq(uint8_t *src, struct ChunkReqInfo *chunkreq) {
    sendChunk(src, chunkreq);
}

#endif

#ifdef STANDALONE_MODE

void sendAssociateReply(const uint8_t *dst) {
    Serial.printf("Sending associate reply...\n");
    uint8_t *data = (uint8_t *)calloc(sizeof(struct AssocInfo) + 1, 1);
    struct AssocInfo *assoc = (struct AssocInfo *)(data + 1);
    *data = PKT_ASSOC_RESP;
    memset(assoc, 0, sizeof(struct AssocInfo));
    assoc->checkinDelay = CHECK_IN_DELAY;
    assoc->retryDelay = RETRY_DELAY;
    assoc->failedCheckinsTillBlank = FAILED_TILL_BLANK;
    assoc->failedCheckinsTillDissoc = FAILED_TILL_REASSOCIATE;
    memcpy(assoc->newKey, preshared_key, 16);
    encodePacket(dst, data, sizeof(struct AssocInfo) + 1);
    free(data);
}

void sendPending(const uint8_t *dst, const struct CheckinInfo *ci) {
    uint8_t *data = (uint8_t *)calloc(sizeof(struct PendingInfo) + 1, 1);
    struct PendingInfo *pi = (struct PendingInfo *)(data + 1);
    *data = PKT_CHECKOUT;
    memset(pi, 0, sizeof(struct PendingInfo));

    // get info about image; timestamp/ver and size
    uint64_t ver;
    uint32_t len;
    String imageFileName = getImageData(ver, len, dst);
    pi->imgUpdateVer = ver;
    pi->imgUpdateSize = len;

    // get info about a potentially available firmware update
    String updateFileName = getUpdateData(ver, len, ci->state.hwType);
    pi->osUpdateVer = ver;
    pi->osUpdateSize = len;

    // send packet out
    encodePacket(dst, data, sizeof(struct PendingInfo) + 1);
    Serial.printf("Sending pending to: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X -> imgVer %lld osVer %lld\n", dst[7], dst[6], dst[5], dst[4], dst[3], dst[2], dst[1], dst[0], pi->imgUpdateVer, pi->osUpdateVer);

    // save information about pending data to array
    if (pi->imgUpdateSize) {
        pendingdata *pendinginfo = pendingdata::findByVer(pi->imgUpdateVer);
        if (pendinginfo == nullptr) {
            pendinginfo = new pendingdata;
            pendinginfo->filename = imageFileName;
            pendinginfo->ver = pi->imgUpdateVer;
            pendinginfo->len = pi->imgUpdateSize;
            pendinginfo->data = nullptr;
            pendingfiles.push_back(pendinginfo);
        } else {
            pendinginfo->timeout = PENDING_TIMEOUT;
        }
    }

    if (pi->osUpdateSize) {
        pendingdata *pendinginfo = pendingdata::findByVer(pi->osUpdateVer);
        if (pendinginfo == nullptr) {
            pendinginfo = new pendingdata;
            pendinginfo->filename = updateFileName;
            pendinginfo->ver = pi->osUpdateVer;
            pendinginfo->len = pi->osUpdateSize;
            pendinginfo->data = nullptr;
            pendingfiles.push_back(pendinginfo);
        } else {
            pendinginfo->timeout = PENDING_TIMEOUT;
        }
    }
    free(data);
}

void sendChunk(const uint8_t *dst, const struct ChunkReqInfo *chunkreq) {
    uint8_t *data = (uint8_t *)calloc(sizeof(struct ChunkInfo) + chunkreq->len + 1, 1);
    struct ChunkInfo *chunk = (struct ChunkInfo *)(data + 1);
    *data = PKT_CHUNK_RESP;
    memset(chunk, 0, sizeof(struct PendingInfo));
    chunk->offset = chunkreq->offset;

    // find info about this chunk version
    pendingdata *pending = pendingdata::findByVer(chunkreq->versionRequested);
    if (pending == nullptr) {
        Serial.printf("Looking for chunk info with version %d but I couldn't find it :(\n", chunkreq->versionRequested);
    } else {
        if (!pending->data) {
            // data not available, get it now.
            Serial.printf("Data not found, getting from filesystem\n");
            downloadFileToBuffer(pending);
            Serial.printf(" - got %d bytes from FS\n", pending->len);
        }
        Serial.printf("Serving offset %d with len %d from buffer - %d%%\n", chunkreq->offset, chunkreq->len, (100 * chunkreq->offset + 1 + chunkreq->len) / pending->len);
        memcpy(chunk->data, &(pending->data[chunkreq->offset]), chunkreq->len);
        chunk->osUpdatePlz = chunkreq->osUpdatePlz;
    }
    encodePacket(dst, data, sizeof(struct ChunkInfo) + chunkreq->len + 1);
    free(data);
}

void processChunkReq(const uint8_t *src, const struct ChunkReqInfo *chunkreq) {
    sendChunk(src, chunkreq);
}
#endif

void processCheckin(const uint8_t *src, const struct CheckinInfo *ci) {
    // process check-in data
    Serial.printf("Check-in from: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X -> Temp: %d - Battery>=%dmV\n", src[7], src[6], src[5], src[4], src[3], src[2], src[1], src[0], ci->temperature - CHECKIN_TEMP_OFFSET, ci->state.batteryMv);
    DynamicJsonDocument checkin(2048);
    char sbuffer[32];
#ifdef STANDALONE_MODE
    char fname[32];
    // Check to see if a state file exists, if it does; load and deserialize
    getStateFileName(src, fname);
    File file;
    if (LittleFS.exists(fname)) {
        file = LittleFS.open(fname, "r");
        DeserializationError error = deserializeJson(checkin, file);
        file.close();
        file = LittleFS.open(fname, "w+");
    } else {
        file = LittleFS.open(fname, "w", true);
    }
#endif
    sprintf(sbuffer, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", src[7], src[6], src[5], src[4], src[3], src[2], src[1], src[0]);
#ifdef NETWORK_MODE
    checkin["type"] = "CHECK_IN";
#endif
    checkin["src"] = sbuffer;
    JsonObject state = checkin.createNestedObject("state");
    state["batteryMv"] = ci->state.batteryMv;
    state["hwType"] = ci->state.hwType;
    state["swVer"] = ci->state.swVer;
    checkin["lastPacketLQI"] = ci->lastPacketLQI;
    checkin["lastPacketRSSI"] = ci->lastPacketRSSI;
    checkin["rfu"] = ci->rfu;  // broken / to-do
    checkin["temperature"] = ci->temperature - CHECKIN_TEMP_OFFSET;
#ifdef STANDALONE_MODE
    sendPending(src, ci);
    // Save State file
    checkin["lastSeen"] = getTime();
    serializeJson(checkin, file);
    file.flush();
    file.close();
#endif
#ifdef NETWORK_MODE
    String json;
    serializeJson(checkin, json);
    // send check-in data from the server, get pending data back
    String pendingdata = postDataToHTTP(json);
    // prepare to send pending data back to the tag
    sendPending(src, pendingdata);
#endif
}

void processAssociateReq(const uint8_t *src, const struct TagInfo *taginfo) {
    Serial.printf("Associate from: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X -> %dx%d (%dx%dmm)\n", src[7], src[6], src[5], src[4], src[3], src[2], src[1], src[0], taginfo->screenPixWidth, taginfo->screenPixHeight, taginfo->screenMmWidth, taginfo->screenMmHeight);
    DynamicJsonDocument assocreq(2048);
    char sbuffer[32];
#ifdef STANDALONE_MODE
    // Check to see if a state file exists, if it does; load and deserialize
    char fname[32];
    getStateFileName(src, fname);
    File file;
    if (LittleFS.exists(fname)) {
        file = LittleFS.open(fname, "r");
        DeserializationError error = deserializeJson(assocreq, file);
        file.close();
        file = LittleFS.open(fname, "w");
    } else {
        file = LittleFS.open(fname, "w", true);
    }
#endif
    // make json data from check-in data from the tag
    sprintf(sbuffer, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", src[7], src[6], src[5], src[4], src[3], src[2], src[1], src[0]);
#ifdef NETWORK_MODE
    assocreq["type"] = "ASSOCREQ";
#endif
    assocreq["src"] = sbuffer;
    assocreq["protover"] = taginfo->protoVer;
    JsonObject state = assocreq.createNestedObject("state");
    state["batteryMv"] = taginfo->state.batteryMv;
    state["hwType"] = taginfo->state.hwType;
    state["swVer"] = taginfo->state.swVer;
    assocreq["screenPixWidth"] = taginfo->screenPixWidth;
    assocreq["screenPixHeight"] = taginfo->screenPixHeight;
    assocreq["screenMmWidth"] = taginfo->screenMmWidth;
    assocreq["screenMmHeight"] = taginfo->screenMmHeight;
    assocreq["compressionsSupported"] = taginfo->compressionsSupported;
    assocreq["maxWaitMsec"] = taginfo->maxWaitMsec;
    assocreq["screenType"] = taginfo->screenType;
    assocreq["rfu"] = taginfo->rfu;  // yeah this is broken

#ifdef STANDALONE_MODE
    sendAssociateReply(src);
    // Save State file
    assocreq["lastSeen"] = getTime();
    serializeJson(assocreq, file);
    file.flush();
    file.close();
#endif

#ifdef NETWORK_MODE
    String json;
    serializeJson(assocreq, json);

    // send check-in data from the server, get pending data back
    String assocreplydata = postDataToHTTP(json);
    sendAssociateReply(src, assocreplydata);
#endif
}

void parsePacket(const uint8_t *src, const uint8_t *data, const uint8_t len) {
    portYIELD();
    switch (data[0]) {
        case PKT_ASSOC_REQ: {
            Serial.printf("Association request received\n");
            const struct TagInfo *taginfo = (const struct TagInfo *)(data + 1);
            processAssociateReq(src, taginfo);
        } break;
        case PKT_ASSOC_RESP: {  // not relevant for a base
            Serial.printf("Association response. Not doing anything with that...\n");
            // const struct AssocInfo *associnfo = (const struct AssocInfo *)(data + 1);
        } break;
        case PKT_CHECKIN: {
            const struct CheckinInfo *ci = (const struct CheckinInfo *)(data + 1);
            processCheckin(src, ci);
        } break;
        case PKT_CHUNK_REQ: {
            const struct ChunkReqInfo *chunkreq = (const struct ChunkReqInfo *)(data + 1);
            processChunkReq(src, chunkreq);
        } break;

        default:
            Serial.printf("Received a frame of type %02X, currently unimplemented :<\n", data[0]);
            break;
    }
}
