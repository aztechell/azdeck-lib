#include "TcpTransport.h"

#include "../JsonStreamFramer.h"
#include "../PlatformCaps.h"
#include "../TextStreamFramer.h"

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
bool gJsonMode = true;
bool gHadClient = false;
AzDeckJsonStreamFramer gJsonFramer;
AzDeckTextStreamFramer gTextFramer;

struct TcpCallbacks {
    void (*onPayload)(void* context, const char* data, size_t length);
    void (*onFail)(void* context);
    void (*onDisconnect)(void* context);
    void* context;
};

TcpCallbacks gCallbacks = {nullptr, nullptr, nullptr, nullptr};

void onJsonObject(void* context, const char* data, size_t length) {
    TcpCallbacks* callbacks = static_cast<TcpCallbacks*>(context);
    if (callbacks != nullptr && callbacks->onPayload != nullptr) {
        callbacks->onPayload(callbacks->context, data, length);
    }
}

void onJsonFail(void* context) {
    TcpCallbacks* callbacks = static_cast<TcpCallbacks*>(context);
    if (callbacks != nullptr && callbacks->onFail != nullptr) {
        callbacks->onFail(callbacks->context);
    }
}

void onTextLine(void* context, const char* data, size_t length) {
    TcpCallbacks* callbacks = static_cast<TcpCallbacks*>(context);
    if (callbacks != nullptr && callbacks->onPayload != nullptr) {
        callbacks->onPayload(callbacks->context, data, length);
    }
}
}  // namespace

bool azdeckTcpBegin(uint16_t port, bool jsonMode) {
    gJsonMode = jsonMode;
    gHadClient = false;
    gJsonFramer.reset();
    gTextFramer.reset();
    gClient.stop();

    if (gServer != nullptr) {
        delete gServer;
        gServer = nullptr;
    }
    gServer = new WiFiServer(port);
    if (gServer == nullptr) {
        return false;
    }
    gServer->begin();
    return true;
}

void azdeckTcpUpdate(
    void (*onPayload)(void* context, const char* data, size_t length),
    void (*onFail)(void* context),
    void (*onDisconnect)(void* context),
    void* context
) {
    gCallbacks.onPayload = onPayload;
    gCallbacks.onFail = onFail;
    gCallbacks.onDisconnect = onDisconnect;
    gCallbacks.context = context;

    if (gServer == nullptr) {
        return;
    }

    WiFiClient incoming = gServer->available();
    if (incoming) {
        if (!gClient || !gClient.connected()) {
            gClient = incoming;
            gJsonFramer.reset();
            gTextFramer.reset();
            gHadClient = true;
        }
    }

    if (!gClient || !gClient.connected()) {
        if (gHadClient) {
            gHadClient = false;
            gJsonFramer.reset();
            gTextFramer.reset();
            if (gCallbacks.onDisconnect != nullptr) {
                gCallbacks.onDisconnect(gCallbacks.context);
            }
        }
        return;
    }

    gHadClient = true;
    bool overflow = false;
    while (gClient.available() > 0) {
        uint8_t chunk[64];
        const int readCount = gClient.read(chunk, sizeof(chunk));
        if (readCount <= 0) {
            break;
        }
        if (gJsonMode) {
            gJsonFramer.feed(
                chunk,
                static_cast<size_t>(readCount),
                onJsonObject,
                onJsonFail,
                &gCallbacks
            );
        } else if (!gTextFramer.append(chunk, static_cast<size_t>(readCount))) {
            overflow = true;
            break;
        }
    }

    if (!gJsonMode) {
        if (overflow) {
            gTextFramer.reset();
            if (gCallbacks.onFail != nullptr) {
                gCallbacks.onFail(gCallbacks.context);
            }
            return;
        }
        gTextFramer.processAfterBurst(onTextLine, onJsonFail, &gCallbacks);
    }
}

void azdeckTcpSend(const char* data, size_t length) {
    if (!gClient || !gClient.connected() || data == nullptr || length == 0) {
        return;
    }
    gClient.write(reinterpret_cast<const uint8_t*>(data), length);
}

#else

bool azdeckTcpBegin(uint16_t port, bool jsonMode) {
    (void)port;
    (void)jsonMode;
    return false;
}

void azdeckTcpUpdate(
    void (*onPayload)(void* context, const char* data, size_t length),
    void (*onFail)(void* context),
    void (*onDisconnect)(void* context),
    void* context
) {
    (void)onPayload;
    (void)onFail;
    (void)onDisconnect;
    (void)context;
}

void azdeckTcpSend(const char* data, size_t length) {
    (void)data;
    (void)length;
}

#endif
