#include <Arduino.h>

void zbsTx(uint8_t* packetdata, uint8_t len, uint8_t retry);
void zbsRxTask(void* parameter);

extern QueueHandle_t outQueue;
extern QueueHandle_t inQueue;

struct serialFrame {
    uint8_t len;
    uint8_t crc;
    uint8_t* data;
} __packed;
