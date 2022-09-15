#include <Arduino.h>

#define PENDING_TIMEOUT 120
#define WEBSERVER_PATH "http://your-webserver/esl/"
#define CHECK_IN_URL "http://your-webserver/esl/check-in.php"



// flasher options
#define CUSTOM_MAC_HDR 0x0000

// connections to the tag
#define RXD1 16
#define TXD1 17
#define ZBS_SS 21
#define ZBS_CLK 18
#define ZBS_MoSi 22
#define ZBS_MiSo 19
#define ZBS_Reset 5
#define ZBS_POWER1 15
#define ZBS_POWER2 2

#define MAX_WRITE_ATTEMPTS 5