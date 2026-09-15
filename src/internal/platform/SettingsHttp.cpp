#include "SettingsHttp.h"

#include "../Config.h"
#include "../PlatformCaps.h"
#include "SettingsHttpProtocol.h"

#include <string.h>

#if AZDECK_HAS_WIFI
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_UNOR4_WIFI)
#include <WiFiS3.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#endif

#if AZDECK_HAS_WIFI

namespace {
WiFiServer* gServer = nullptr;
WiFiClient gClient;
const char* gHtml = nullptr;
void (*gOnQuery)(const char* query) = nullptr;
bool gHaveClient = false;
size_t gReqLen = 0;
#if defined(ESP8266)
static char gReq[AZDECK_HTTP_REQUEST_BUFFER];
static char gHeaders[256];
static char gQuery[64];
#else
char gReq[AZDECK_HTTP_REQUEST_BUFFER];
char gHeaders[256];
char gQuery[64];
#endif

void closeClient() {
    if (gHaveClient) {
        gClient.stop();
        gHaveClient = false;
    }
    gReqLen = 0;
}

void reply(const char* headers, size_t headerLength, const char* body, size_t bodyLength) {
    if (!gHaveClient || headers == nullptr || headerLength == 0) {
        closeClient();
        return;
    }
    gClient.write(reinterpret_cast<const uint8_t*>(headers), headerLength);
    if (body != nullptr && bodyLength > 0) {
        gClient.write(reinterpret_cast<const uint8_t*>(body), bodyLength);
    }
    closeClient();
#if defined(ESP8266)
    yield();
#endif
}

void handleRequest() {
    const AzDeckSettingsHttpKind kind = azdeckSettingsHttpClassify(gReq, gReqLen);
    if (kind == AZDECK_HTTP_INCOMPLETE) {
        if (gReqLen + 1 >= sizeof(gReq)) {
            closeClient();
        }
        return;
    }

    const size_t bodyLength = (gHtml != nullptr) ? strlen(gHtml) : 0;
    if (kind == AZDECK_HTTP_SETTINGS_HEAD || kind == AZDECK_HTTP_SETTINGS_GET) {
        if (kind == AZDECK_HTTP_SETTINGS_GET && gOnQuery != nullptr) {
            if (azdeckSettingsHttpQuery(gReq, gReqLen, gQuery, sizeof(gQuery)) > 0) {
                gOnQuery(gQuery);
            }
        }
        const size_t n = azdeckSettingsHttpFormatOk(gHeaders, sizeof(gHeaders), bodyLength);
        const char* body =
            (kind == AZDECK_HTTP_SETTINGS_GET) ? gHtml : nullptr;
        const size_t sendBody =
            (kind == AZDECK_HTTP_SETTINGS_GET) ? bodyLength : 0;
        reply(gHeaders, n, body, sendBody);
        return;
    }

    const size_t n = azdeckSettingsHttpFormatNotFound(gHeaders, sizeof(gHeaders));
    reply(gHeaders, n, nullptr, 0);
}
}  // namespace

bool azdeckSettingsHttpBegin(const char* html, void (*onQuery)(const char* query)) {
    if (html == nullptr || html[0] == '\0') {
        return true;
    }
    gHtml = html;
    gOnQuery = onQuery;
    gReqLen = 0;
    gHaveClient = false;
    if (gServer == nullptr) {
        gServer = new WiFiServer(AZDECK_SETTINGS_HTTP_PORT);
        if (gServer == nullptr) {
            gHtml = nullptr;
            return false;
        }
    }
    gServer->begin();
    return true;
}

void azdeckSettingsHttpUpdate() {
    if (gServer == nullptr || gHtml == nullptr) {
        return;
    }

    WiFiClient incoming = gServer->available();
    if (incoming) {
        closeClient();
        gClient = incoming;
        gHaveClient = true;
        gReqLen = 0;
    }

    if (!gHaveClient) {
        return;
    }
    if (!gClient.connected()) {
        closeClient();
        return;
    }

    while (gClient.available() > 0) {
        if (gReqLen + 1 >= sizeof(gReq)) {
            closeClient();
            return;
        }
        const int c = gClient.read();
        if (c < 0) {
            break;
        }
        gReq[gReqLen++] = static_cast<char>(c);
        gReq[gReqLen] = '\0';
        if (gReqLen >= 4 &&
            gReq[gReqLen - 4] == '\r' && gReq[gReqLen - 3] == '\n' &&
            gReq[gReqLen - 2] == '\r' && gReq[gReqLen - 1] == '\n') {
            handleRequest();
            return;
        }
    }
}

#else

bool azdeckSettingsHttpBegin(const char* html, void (*onQuery)(const char* query)) {
    (void)html;
    (void)onQuery;
    return true;
}

void azdeckSettingsHttpUpdate() {}

#endif
