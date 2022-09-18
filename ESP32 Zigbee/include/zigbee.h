#include <Arduino.h>

#define PROTO_PRESHARED_KEY                                                                                            \
    {                                                                                                                  \
        0x34D906D3, 0xE3E5298E, 0x3429BF58, 0xC1022081                                                                 \
    }

#define PROTO_PAN_ID (0x4447) // PAN ID compression shall be used

#pragma pack(push, 1)

enum macframetype
{
    MACFRAME_TYPE_MASTER,
    MACFRAME_TYPE_NORMAL,
    MACFRAME_TYPE_BROADCAST
};

extern uint8_t devicemac[8];
extern uint32_t preshared_key[4];

struct MacFcs
{
    uint8_t frameType : 3;
    uint8_t secure : 1;
    uint8_t framePending : 1;
    uint8_t ackReqd : 1;
    uint8_t panIdCompressed : 1;
    uint8_t rfu1 : 1;
    uint8_t rfu2 : 2;
    uint8_t destAddrType : 2;
    uint8_t frameVer : 2;
    uint8_t srcAddrType : 2;
};

struct MacFrameFromMaster
{
    struct MacFcs fcs;
    uint8_t seq;
    uint16_t pan;
    uint8_t dst[8];
    uint16_t from;
};

struct MacFrameNormal
{
    struct MacFcs fcs;
    uint8_t seq;
    uint16_t pan;
    uint8_t dst[8];
    uint8_t src[8];
};

struct MacFrameBcast
{
    struct MacFcs fcs;
    uint8_t seq;
    uint16_t dstPan;
    uint16_t dstAddr;
    uint16_t srcPan;
    uint8_t src[8];
};

void decodePacket(const uint8_t* p, const uint8_t len);
void encodePacket(const uint8_t* dst, uint8_t* data, const uint8_t len);
void dumpHex(void* p, uint16_t len);