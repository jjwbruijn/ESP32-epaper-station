#include <Arduino.h>

//#define NETWORK_MODE        // comment me in to enable network-mode. I'm ready to go in, coach, just give me a chance!

#ifndef NETWORK_MODE
    #define STANDALONE_MODE
#endif

#ifdef NETWORK_MODE
#define WEBSERVER_PATH "http://your-webserver/esl/"
#define CHECK_IN_URL "http://your-webserver/esl/check-in.php"
#endif

#ifdef STANDALONE_MODE
    #define CHECK_IN_DELAY 900000
    #define RETRY_DELAY 1000
    #define FAILED_TILL_BLANK 2
    #define FAILED_TILL_REASSOCIATE 1
#endif

// how many times we should try to send a packet
#define ZIGBEE_ATTEMPTS 5

// this determines how long images will be cached;
#define PENDING_TIMEOUT 120


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