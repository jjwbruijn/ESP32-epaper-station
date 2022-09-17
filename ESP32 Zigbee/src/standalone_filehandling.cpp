#include "standalone_filehandling.h"

#include <LittleFS.h>

#include "pendingdata.h"
#include "tags-custom-proto.h"

#define VERSION_SIGNIFICANT_MASK (0x0000ffffffffffff)

#define HW_TYPE_42_INCH_SAMSUNG (1)
#define HW_TYPE_42_INCH_SAMSUNG_ROM_VER_OFST (0xEFF8)
#define HW_TYPE_74_INCH_DISPDATA (2)
#define HW_TYPE_74_INCH_DISPDATA_FRAME_MODE (3)
#define HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST (0x008b)
#define HW_TYPE_ZBD_EPOP50 (4)
#define HW_TYPE_ZBD_EPOP50_ROM_VER_OFST (0x008b)
#define HW_TYPE_ZBD_EPOP900 (5)
#define HW_TYPE_ZBD_EPOP900_ROM_VER_OFST (0x008b)
#define HW_TYPE_29_INCH_DISPDATA (6)
#define HW_TYPE_29_INCH_DISPDATA_FRAME_MODE (7)
#define HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST (0x008b)
#define HW_TYPE_29_INCH_ZBS_026 (8)
#define HW_TYPE_29_INCH_ZBS_026_FRAME_MODE (9)
#define HW_TYPE_29_INCH_ZBS_025 (10)
#define HW_TYPE_29_INCH_ZBS_025_FRAME_MODE (11)
#define HW_TYPE_154_INCH_ZBS_033 (12)
#define HW_TYPE_29_INCH_ZBS_ROM_VER_OFST (0x008b)

uint16_t getFirmwareVersionOffset(uint8_t hwType) {
    switch (hwType) {
        case HW_TYPE_42_INCH_SAMSUNG:
            return HW_TYPE_42_INCH_SAMSUNG_ROM_VER_OFST;
        case HW_TYPE_74_INCH_DISPDATA:
            return HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST;
        case HW_TYPE_74_INCH_DISPDATA_FRAME_MODE:
            return HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST;
        case HW_TYPE_29_INCH_DISPDATA:
            return HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST;
        case HW_TYPE_29_INCH_DISPDATA_FRAME_MODE:
            return HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST;
        case HW_TYPE_ZBD_EPOP50:
            return HW_TYPE_ZBD_EPOP50_ROM_VER_OFST;
        case HW_TYPE_ZBD_EPOP900:
            return HW_TYPE_ZBD_EPOP900_ROM_VER_OFST;
        case HW_TYPE_29_INCH_ZBS_026:
        case HW_TYPE_29_INCH_ZBS_026_FRAME_MODE:
        case HW_TYPE_29_INCH_ZBS_025:
        case HW_TYPE_29_INCH_ZBS_025_FRAME_MODE:
            return HW_TYPE_29_INCH_ZBS_ROM_VER_OFST;
    }
}

File getFileForMac(uint8_t* dst) {
    char buffer[32];
    memset(buffer, 0, 32);
    sprintf(buffer, "/%02X%02X%02X%02X%02X%02X%02X%02X.bmp", dst[7], dst[6], dst[5], dst[4], dst[3], dst[2], dst[1], dst[0]);
    File file = LittleFS.open(buffer);
    return file;
}

String getUpdateData(uint64_t& ver, uint32_t& len, uint8_t hwType) {
    char buffer[16];
    sprintf(buffer, "/UPDT%04X.BIN", hwType);
    if (LittleFS.exists(buffer)) {
        File file = LittleFS.open(buffer);
        file.seek(getFirmwareVersionOffset(hwType));
        uint64_t osVer = 0;
        file.readBytes((char*)(&osVer), 8);
        ver = osVer & VERSION_SIGNIFICANT_MASK | hwType << 48;
        len = file.size();
        file.close();
        return String(buffer);
    } else {
        ver = 0;
        len = 0;
        return "";
    }
}

String getImageData(uint64_t& ver, uint32_t& len, uint8_t* dst) {
    char buffer[32];
    memset(buffer, 0, 32);
    sprintf(buffer, "/%02X%02X%02X%02X%02X%02X%02X%02X.bmp", dst[7], dst[6], dst[5], dst[4], dst[3], dst[2], dst[1], dst[0]);
    if (LittleFS.exists(buffer)) {
        File file = LittleFS.open(buffer);
        ver = (((uint64_t)(file.getLastWrite())) << 32) | ((uint64_t)(file.getLastWrite()));
        len = file.size();
        file.close();
        return String(buffer);
        ;
    } else {
        ver = 0;
        len = 0;
        return "";
    }
}

void downloadFileToBuffer(pendingdata* pending) {
    Serial.print(pending->filename + " was requested from filesystem\n");
    pending->data = (uint8_t*)calloc(pending->len, 1);
    File file = LittleFS.open(pending->filename);
    pending->data = (uint8_t*)calloc(file.size(), 1);
    uint32_t index = 0;
    while (file.available()) {
        pending->data[index] = file.read();
        index++;
        if ((index % 256) == 0) portYIELD();
    }
    file.close();
}