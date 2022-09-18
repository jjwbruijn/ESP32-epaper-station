#include <Arduino.h>
#include "pendingdata.h"

#pragma pack(push, 1)

void downloadFileToBuffer(pendingdata* pending);
String getImageData(uint64_t& ver, uint32_t& len, const uint8_t* dst);
String getUpdateData(uint64_t& ver, uint32_t& len, const uint8_t hwType);
void getStateFileName(const uint8_t *mac, char *buffer);