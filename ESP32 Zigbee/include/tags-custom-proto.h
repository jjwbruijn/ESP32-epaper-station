#include <Arduino.h>
#pragma pack(push, 1);

#define PKT_ASSOC_REQ			(0xF0)
#define PKT_ASSOC_RESP			(0xF1)
#define PKT_CHECKIN				(0xF2)
#define PKT_CHECKOUT			(0xF3)
#define PKT_CHUNK_REQ			(0xF4)
#define PKT_CHUNK_RESP			(0xF5)

struct TagState {
	uint64_t swVer;
	uint16_t hwType;
	uint16_t batteryMv;
} __packed;

struct TagInfo {
	uint8_t protoVer;						//PROTO_VER_*
	struct TagState state;
	uint8_t rfu1[1];						//shall be ignored for now
	uint16_t screenPixWidth;
	uint16_t screenPixHeight;
	uint16_t screenMmWidth;
	uint16_t screenMmHeight;
	uint16_t compressionsSupported;			//COMPR_TYPE_* bitfield
	uint16_t maxWaitMsec;					//how long tag will wait for packets before going to sleep
	uint8_t screenType;						//enum TagScreenType
	uint8_t rfu[11];						//shall be zero for now
} __packed;

struct AssocInfo {
	uint32_t checkinDelay;					//space between checkins, in msec
	uint32_t retryDelay;					//if download fails mid-way wait thi smany msec to retry (IFF progress was made)
	uint16_t failedCheckinsTillBlank;		//how many fails till we go blank
	uint16_t failedCheckinsTillDissoc;		//how many fails till we dissociate
	uint32_t newKey[4];
	uint8_t rfu[8];							//shall be zero for now
} __packed;

#define CHECKIN_TEMP_OFFSET					0x7f

struct CheckinInfo {
	struct TagState state;
	uint8_t lastPacketLQI;					//zero if not reported/not supported to be reported
	int8_t lastPacketRSSI;					//zero if not reported/not supported to be reported
	uint8_t temperature;					//zero if not reported/not supported to be reported. else, this minus CHECKIN_TEMP_OFFSET is temp in degrees C
	uint8_t rfu[6];							//shall be zero for now
} __packed;

struct PendingInfo {
	uint64_t imgUpdateVer;
	uint32_t imgUpdateSize;
	uint64_t osUpdateVer;					//version of OS update avail
	uint32_t osUpdateSize;
	uint8_t rfu[8];							//shall be zero for now
} __packed;

struct ChunkReqInfo {
	uint64_t versionRequested;
	uint32_t offset;
	uint8_t len;
	uint8_t osUpdatePlz	: 1;
	uint8_t rfu[6];							//shall be zero for now
} __packed;

struct ChunkInfo {
	uint32_t offset;
	uint8_t osUpdatePlz	: 1;
	uint8_t rfu;							//shall be zero for now
	uint8_t data[];							//no data means request is out of bounds of this version no longer exists
} __packed;


void parsePacket(uint8_t* src, void* data, uint8_t len);

void sendAssociateReply(uint8_t* dst, String associatereplydata);
void sendPending(uint8_t* dst, String pendingdatastr);
void sendChunk(uint8_t* dst, struct ChunkReqInfo* chunkreq);
void processCheckin(uint8_t* src, struct CheckinInfo* ci);
void processAssociateReq(uint8_t* src, struct TagInfo* taginfo);
void processChunkReq(uint8_t* src, struct ChunkReqInfo* chunkreq);