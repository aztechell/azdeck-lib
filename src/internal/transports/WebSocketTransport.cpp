#include "WebSocketTransport.h"

#include "../Inbox.h"
#include "../PlatformCaps.h"

#if AZDECK_HAS_WIFI
#include <WebSocketsServer.h>
#endif

#if AZDECK_HAS_WIFI

namespace {
WebSocketsServer* gWebSocket = nullptr;
AzDeckInbox gInbox;
bool gStarted = false;

void onWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            break;
        case WStype_DISCONNECTED:
            gInbox.disconnect = true;
            break;
        case WStype_TEXT:
        case WStype_BIN:
            azdeckInboxPush(gInbox, payload, length, clientId);
            break;
        default:
            break;
    }
}
}  // namespace

bool azdeckWsBegin(uint16_t port) {
    azdeckInboxInit(gInbox);
    if (gWebSocket != nullptr) {
        delete gWebSocket;
        gWebSocket = nullptr;
        gStarted = false;
    }
    gWebSocket = new WebSocketsServer(port);
    if (gWebSocket == nullptr) {
        return false;
    }
    gWebSocket->begin();
    gWebSocket->onEvent(onWebSocketEvent);
    gStarted = true;
    return true;
}

void azdeckWsUpdate() {
    if (gWebSocket != nullptr) {
        gWebSocket->loop();
    }
}

bool azdeckWsTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    uint8_t* clientId,
    bool* overflow
) {
    return azdeckInboxTake(gInbox, buffer, capacity, outLength, clientId, overflow);
}

bool azdeckWsTakeDisconnect() {
    return azdeckInboxTakeDisconnect(gInbox);
}

void azdeckWsSend(uint8_t clientId, const char* data, size_t length) {
    if (gWebSocket == nullptr || data == nullptr || length == 0) {
        return;
    }
    gWebSocket->sendTXT(clientId, data, length);
}

#else

bool azdeckWsBegin(uint16_t port) {
    (void)port;
    return false;
}

void azdeckWsUpdate() {}

bool azdeckWsTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    uint8_t* clientId,
    bool* overflow
) {
    (void)buffer;
    (void)capacity;
    (void)outLength;
    (void)clientId;
    if (overflow != nullptr) {
        *overflow = false;
    }
    return false;
}

bool azdeckWsTakeDisconnect() {
    return false;
}

void azdeckWsSend(uint8_t clientId, const char* data, size_t length) {
    (void)clientId;
    (void)data;
    (void)length;
}

#endif
