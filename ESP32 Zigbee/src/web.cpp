#include "web.h"

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <SPIFFSEditor.h>
#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager/tree/feature_asyncwebserver

#include <LittleFS.h>

#include "settings.h"

extern uint8_t data_to_send[];

const char *http_username = "admin";
const char *http_password = "admin";
AsyncWebServer server(80);

void WriteBMP(const char *filename, uint8_t *pData, int width, int height, int bpp) {
    int lsize = 0, i, iHeaderSize, iBodySize;
    uint8_t pBuf[128];  // holds BMP header
    File file_out = LittleFS.open(filename, "wb");

    lsize = (lsize + 3) & 0xfffc;  // DWORD aligned
    iHeaderSize = 54;
    iHeaderSize += (1 << (bpp + 2));
    iBodySize = lsize * height;
    i = iBodySize + iHeaderSize;  // datasize
    memset(pBuf, 0, 54);
    pBuf[0] = 'B';
    pBuf[1] = 'M';
    pBuf[2] = i & 0xff;  // 4 bytes of file size
    pBuf[3] = (i >> 8) & 0xff;
    pBuf[4] = (i >> 16) & 0xff;
    pBuf[5] = (i >> 24) & 0xff;
    /* Offset to data bits */
    pBuf[10] = iHeaderSize & 0xff;
    pBuf[11] = (unsigned char)(iHeaderSize >> 8);
    pBuf[14] = 0x28;
    pBuf[18] = width & 0xff;                 // xsize low
    pBuf[19] = (unsigned char)(width >> 8);  // xsize high
    i = -height;                             // top down bitmap
    pBuf[22] = i & 0xff;                     // ysize low
    pBuf[23] = (unsigned char)(i >> 8);      // ysize high
    pBuf[24] = 0xff;
    pBuf[25] = 0xff;
    pBuf[26] = 1;  // number of planes
    pBuf[28] = (uint8_t)bpp;
    pBuf[30] = 0;  // uncompressed
    i = iBodySize;
    pBuf[34] = i & 0xff;  // data size
    pBuf[35] = (i >> 8) & 0xff;
    pBuf[36] = (i >> 16) & 0xff;
    pBuf[37] = (i >> 24) & 0xff;
    pBuf[54] = pBuf[55] = pBuf[56] = pBuf[57] = pBuf[61] = 0;  // palette
    pBuf[58] = pBuf[59] = pBuf[60] = 0xff;
    {
        uint8_t *s = pData;
        file_out.write(pBuf, iHeaderSize);
        for (i = 0; i < height; i++) {
            file_out.write(s, lsize);
            s += lsize;
        }
        file_out.close();
    }
} /* WriteBMP() */

void init_web() {
    LittleFS.begin(true);
    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("AutoConnectAP");
    if (!res) {
        Serial.println("Failed to connect");
        ESP.restart();
    }
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());

    server.addHandler(new SPIFFSEditor(LittleFS, http_username, http_password));

    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });


    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "OK Reboot");
        ESP.restart();
    });

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.htm");

    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->url() == "/" || request->url() == "index.htm") { 
            request->send(200, "text/html", "-");
            return;
        }
        Serial.printf("NOT_FOUND: ");
        if (request->method() == HTTP_GET)
            Serial.printf("GET");
        else if (request->method() == HTTP_POST)
            Serial.printf("POST");
        else if (request->method() == HTTP_DELETE)
            Serial.printf("DELETE");
        else if (request->method() == HTTP_PUT)
            Serial.printf("PUT");
        else if (request->method() == HTTP_PATCH)
            Serial.printf("PATCH");
        else if (request->method() == HTTP_HEAD)
            Serial.printf("HEAD");
        else if (request->method() == HTTP_OPTIONS)
            Serial.printf("OPTIONS");
        else
            Serial.printf("UNKNOWN");
        Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

        if (request->contentLength()) {
            Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
            Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
        }
        int headers = request->headers();
        int i;
        for (i = 0; i < headers; i++) {
            AsyncWebHeader *h = request->getHeader(i);
            Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
        }
        int params = request->params();
        for (i = 0; i < params; i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->isFile()) {
                Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
            } else if (p->isPost()) {
                Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            } else {
                Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }
        request->send(404);
    });

    server.begin();
}
