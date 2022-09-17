#include "tags-custom-proto.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "http.h" // <<
#include "pendingdata.h"
#include "settings.h"
#include "standalone_filehandling.h"
#include "testimage.h"
#include "time.h"
#include "zigbee.h" // <<

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
  assoc->rfu[0] = assocreply["rfu"]; // broken

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
  uint8_t *data =
      (uint8_t *)calloc(sizeof(struct ChunkInfo) + chunkreq->len + 1, 1);
  struct ChunkInfo *chunk = (struct ChunkInfo *)(data + 1);
  *data = PKT_CHUNK_RESP;
  memset(chunk, 0, sizeof(struct PendingInfo));
  chunk->offset = chunkreq->offset;

  // find info about this chunk version
  pendingdata *pending = pendingdata::findByVer(chunkreq->versionRequested);
  if (pending == nullptr) {
    Serial.printf(
        "Looking for chunk info with version %d but I couldn't find it :(\n",
        chunkreq->versionRequested);
  } else {
    if (!pending->data) {
      // data not available, get it now.
      Serial.printf("Data not found, asking the webserver");
      pending->len = getDataFromHTTP(&(pending->data), pending->filename);
      Serial.printf(" - got %d bytes from server\n", pending->len);
    }
    Serial.printf("Serving offset %d from buffer - %d%%\n", chunkreq->offset,
                  (100 * chunkreq->offset + 1 + chunkreq->len) / pending->len);
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

/// @brief Get the image filename for a tag with the given mac
/// @param mac MAC address of the tag
/// @param buffer Buffer to fill with filename
void getImageFileName(const uint8_t *mac, char *buffer) {
  sprintf(buffer, "/%02X%02X%02X%02X%02X%02X%02X%02X.bmp", mac[7], mac[6],
          mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}

/// @brief Get the state filename for a tag with the given mac
/// @param mac MAC address of the tag
/// @param buffer Buffer to fill with filename
void getStateFileName(const uint8_t *mac, char *buffer) {
  sprintf(buffer, "/state_%02X%02X%02X%02X%02X%02X%02X%02X.json", mac[7],
          mac[6], mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}

File getFileForMac(const uint8_t *dst) {
  char buffer[32];
  getImageFileName(dst, buffer);
  File file = LittleFS.open(buffer);
  return file;
}

void downloadFileToBuffer(pendingdata *pending) {
  pending->data = (uint8_t *)calloc(pending->len, 1);
  File file = LittleFS.open(pending->filename);
  pending->data = (uint8_t *)calloc(file.size(), 1);
  uint32_t index = 0;
  while (file.available()) {
    pending->data[index] = file.read();
    index++;
    if ((index % 256) == 0)
      portYIELD();
  }
}

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

void sendPending(const uint8_t *dst, struct CheckinInfo *ci) {
  Serial.printf("Sending pending...\n");
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
  uint8_t *data =
      (uint8_t *)calloc(sizeof(struct ChunkInfo) + chunkreq->len + 1, 1);
  struct ChunkInfo *chunk = (struct ChunkInfo *)(data + 1);
  *data = PKT_CHUNK_RESP;
  memset(chunk, 0, sizeof(struct PendingInfo));
  chunk->offset = chunkreq->offset;

  // find info about this chunk version
  pendingdata *pending = pendingdata::findByVer(chunkreq->versionRequested);
  if (pending == nullptr) {
    Serial.printf(
        "Looking for chunk info with version %d but I couldn't find it :(\n",
        chunkreq->versionRequested);
  } else {
    if (!pending->data) {
      // data not available, get it now.
      Serial.printf("Data not found, getting from filesystem\n");
      downloadFileToBuffer(pending);
      Serial.printf(" - got %d bytes from FS\n", pending->len);
    }
    Serial.printf("Serving offset %d with len %d from buffer - %d%%\n",
                  chunkreq->offset, chunkreq->len,
                  (100 * chunkreq->offset + 1 + chunkreq->len) / pending->len);
    memcpy(chunk->data, &(pending->data[chunkreq->offset]), chunkreq->len);
    Serial.printf("osUpdatePlz=%d\n", chunkreq->osUpdatePlz);
    if (chunkreq->osUpdatePlz)
      chunk->osUpdatePlz = 1;
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
  Serial.printf("Check-in: Battery = %d\n", ci->state.batteryMv);
  char sbuffer[32];

  StaticJsonDocument<1200> checkin;

#ifdef STANDALONE_MODE
  // Check to see if a state file exists, if it does; load and deserialize
  getStateFileName(src, sbuffer);
  File file;
  if (LittleFS.exists(sbuffer)) {
    file = LittleFS.open(sbuffer, "w");
    DeserializationError error = deserializeJson(checkin, file);
  } else {
    file = LittleFS.open(sbuffer, "w", true);
  }
#endif

  sprintf(sbuffer, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", src[7], src[6],
          src[5], src[4], src[3], src[2], src[1], src[0]);
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
  checkin["rfu"] = ci->rfu; // broken / to-do
  checkin["temperature"] = ci->temperature;

#ifdef STANDALONE_MODE
  // Save State file
  checkin["lastSeen"] = getTime();
  serializeJson(checkin, file);
  file.close();
  sendPending(src, ci);
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
  Serial.printf("Tag %dx%d (%dx%dmm)\n", taginfo->screenPixWidth,
                taginfo->screenPixHeight, taginfo->screenMmWidth,
                taginfo->screenMmHeight);

  char sbuffer[32];

  StaticJsonDocument<1200> assocreq;
#ifdef STANDALONE_MODE
  // Check to see if a state file exists, if it does; load and deserialize
  getStateFileName(src, sbuffer);
  File file;
  if (LittleFS.exists(sbuffer)) {
    file = LittleFS.open(sbuffer, "w");
    DeserializationError error = deserializeJson(assocreq, file);
  } else {
    file = LittleFS.open(sbuffer, "w", true);
  }
#endif

  // make json data from check-in data from the tag
  sprintf(sbuffer, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", src[7], src[6],
          src[5], src[4], src[3], src[2], src[1], src[0]);
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
  assocreq["rfu"] = taginfo->rfu; // yeah this is broken

#ifdef STANDALONE_MODE
  // Save State file
  assocreq["lastSeen"] = getTime();
  serializeJson(assocreq, file);
  file.close();
  sendAssociateReply(src);
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
  if (data[0] == PKT_ASSOC_REQ) {
    Serial.printf("Association request received\n");
    const struct TagInfo *taginfo = (const struct TagInfo *)(data + 1);
    processAssociateReq(src, taginfo);
  } else if (data[0] == PKT_ASSOC_RESP) { // not relevant for a base
    Serial.printf("Association response. Not doing anything with that...\n");
    const struct AssocInfo *associnfo = (const struct AssocInfo *)(data + 1);
  } else if (data[0] == PKT_CHECKIN) {
    const struct CheckinInfo *ci = (const struct CheckinInfo *)(data + 1);
    processCheckin(src, ci);
  } else if (data[0] == PKT_CHUNK_REQ) {
    const struct ChunkReqInfo *chunkreq =
        (const struct ChunkReqInfo *)(data + 1);
    processChunkReq(src, chunkreq);
  } else {
    Serial.printf("Received a frame of type %02X, currently unimplemented :<\n",
                  data[0]);
  }
}
