#include "AzDeck.h"

#include "internal/Config.h"
#include "internal/JsonParser.h"
#include "internal/Ping.h"
#include "internal/PlatformCaps.h"
#include "internal/TextParser.h"
#include "internal/platform/WifiAp.h"
#include "internal/transports/BleTransport.h"
#include "internal/transports/SppTransport.h"
#include "internal/transports/TcpTransport.h"
#include "internal/transports/WebSocketTransport.h"

#include <string.h>

namespace {

void copyCString(char* dest, size_t capacity, const char* src) {
    if (dest == nullptr || capacity == 0) {
        return;
    }
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }
    size_t n = 0;
    while (n + 1 < capacity && src[n] != '\0') {
        dest[n] = src[n];
        ++n;
    }
    dest[n] = '\0';
}

bool transportSupported(AzDeckTransport transport) {
    switch (transport) {
        case BLE:
            return AZDECK_HAS_BLE;
        case WEBSOCKET:
        case TCP:
            return AZDECK_HAS_WIFI;
        case SPP:
            return AZDECK_HAS_SPP;
        default:
            return false;
    }
}

}  // namespace

AzDeck::AzDeck()
    : transport_(BLE),
      serializer_(JSON),
      timeoutMs_(AZDECK_DEFAULT_TIMEOUT_MS),
      lastCommandMs_(0),
      started_(false),
      hasCommand_(false),
      wifiConfigured_(false),
      netPort_(AZDECK_DEFAULT_WS_PORT) {
    copyCString(deviceName_, sizeof(deviceName_), AZDECK_DEFAULT_NAME);
    copyCString(ssid_, sizeof(ssid_), AZDECK_DEFAULT_SSID);
    copyCString(password_, sizeof(password_), AZDECK_DEFAULT_PASSWORD);
}

void AzDeck::name(const char* deviceName) {
    if (deviceName == nullptr || deviceName[0] == '\0') {
        return;
    }
    copyCString(deviceName_, sizeof(deviceName_), deviceName);
}

void AzDeck::wifi(const char* ssid, const char* password, uint16_t port) {
    if (ssid == nullptr || ssid[0] == '\0') {
        return;
    }
    copyCString(ssid_, sizeof(ssid_), ssid);
    copyCString(password_, sizeof(password_), password != nullptr ? password : "");
    netPort_ = port;
    wifiConfigured_ = true;
}

bool AzDeck::begin(
    AzDeckTransport transport,
    AzDeckSerializer serializer,
    uint16_t timeoutMs
) {
    if (started_) {
        return false;
    }
    if (!transportSupported(transport)) {
        return false;
    }

    transport_ = transport;
    serializer_ = serializer;
    timeoutMs_ = timeoutMs;
    lastCommandMs_ = 0;
    hasCommand_ = false;
    channels_.zeroValues();

    if (!startTransport()) {
        return false;
    }

    started_ = true;
    return true;
}

bool AzDeck::startTransport() {
    switch (transport_) {
        case BLE:
            return azdeckBleBegin(deviceName_);
        case SPP:
            return azdeckSppBegin(deviceName_);
        case WEBSOCKET: {
            const uint16_t port = wifiConfigured_ ? netPort_ : AZDECK_DEFAULT_WS_PORT;
            if (!azdeckStartAccessPoint(ssid_, password_)) {
                return false;
            }
            return azdeckWsBegin(port);
        }
        case TCP: {
            const uint16_t port = wifiConfigured_ ? netPort_ : AZDECK_DEFAULT_TCP_PORT;
            if (!azdeckStartAccessPoint(ssid_, password_)) {
                return false;
            }
            return azdeckTcpBegin(port, serializer_ == JSON);
        }
        default:
            return false;
    }
}

void AzDeck::failsafe() {
    channels_.zeroValues();
}

void AzDeck::sendReply(const char* data, size_t length, uint8_t clientId) {
    switch (transport_) {
        case WEBSOCKET:
            azdeckWsSend(clientId, data, length);
            break;
        case BLE:
            azdeckBleSend(data, length);
            break;
        case TCP:
            azdeckTcpSend(data, length);
            break;
        case SPP:
            azdeckSppSend(data, length);
            break;
        default:
            break;
    }
}

void AzDeck::handlePayload(const char* data, size_t length, uint8_t clientId) {
    if (data == nullptr || length == 0) {
        return;
    }

    if (azdeckIsPing(data, length)) {
        char pong[AZDECK_RX_BUFFER_SIZE];
        size_t pongLength = 0;
        if (azdeckBuildPong(data, length, pong, sizeof(pong), &pongLength)) {
            sendReply(pong, pongLength, clientId);
        }
        return;
    }

    if (serializer_ == JSON) {
        const AzDeckJsonParseResult result = azdeckParseJson(data, length, channels_);
        if (result == AZDECK_JSON_MALFORMED) {
            failsafe();
            return;
        }
        if (result == AZDECK_JSON_SERVICE) {
            return;
        }
        lastCommandMs_ = millis();
        hasCommand_ = true;
        return;
    }

    const AzDeckTextParseResult result = azdeckParseText(data, length, channels_);
    if (result == AZDECK_TEXT_MALFORMED) {
        failsafe();
        return;
    }
    lastCommandMs_ = millis();
    hasCommand_ = true;
}

void AzDeck::tcpPayloadThunk(void* context, const char* data, size_t length) {
    static_cast<AzDeck*>(context)->handlePayload(data, length, 0);
}

void AzDeck::tcpFailThunk(void* context) {
    static_cast<AzDeck*>(context)->failsafe();
}

void AzDeck::tcpDisconnectThunk(void* context) {
    AzDeck* self = static_cast<AzDeck*>(context);
    self->failsafe();
    self->hasCommand_ = false;
}

void AzDeck::checkTimeout() {
    if (!hasCommand_ || timeoutMs_ == 0) {
        return;
    }
    if (millis() - lastCommandMs_ > timeoutMs_) {
        failsafe();
        hasCommand_ = false;
    }
}

void AzDeck::pollTransport() {
    char buffer[AZDECK_RX_BUFFER_SIZE];
    size_t length = 0;
    uint8_t clientId = 0;
    bool overflow = false;

    switch (transport_) {
        case BLE:
            azdeckBleUpdate();
            if (azdeckBleTakeDisconnect()) {
                failsafe();
                hasCommand_ = false;
            }
            while (azdeckBleTake(buffer, sizeof(buffer), &length, &overflow)) {
                handlePayload(buffer, length, 0);
            }
            if (overflow) {
                failsafe();
            }
            break;
        case WEBSOCKET:
            azdeckWsUpdate();
            if (azdeckWsTakeDisconnect()) {
                failsafe();
                hasCommand_ = false;
            }
            while (azdeckWsTake(buffer, sizeof(buffer), &length, &clientId, &overflow)) {
                handlePayload(buffer, length, clientId);
            }
            if (overflow) {
                failsafe();
            }
            break;
        case TCP:
            azdeckTcpUpdate(
                tcpPayloadThunk,
                tcpFailThunk,
                tcpDisconnectThunk,
                this
            );
            break;
        case SPP:
            azdeckSppUpdate();
            if (azdeckSppTakeDisconnect()) {
                failsafe();
                hasCommand_ = false;
            }
            while (azdeckSppTake(buffer, sizeof(buffer), &length, &overflow)) {
                handlePayload(buffer, length, 0);
            }
            if (overflow) {
                failsafe();
            }
            break;
        default:
            break;
    }
}

void AzDeck::update() {
    if (!started_) {
        return;
    }
    pollTransport();
    checkTimeout();
}

float AzDeck::value(const char* channel) const {
    return channels_.get(channel);
}

float AzDeck::axis(const char* channel) const {
    return value(channel);
}

float AzDeck::slider(const char* channel) const {
    return value(channel);
}

bool AzDeck::button(const char* channel) const {
    return value(channel) != 0.0f;
}

bool AzDeck::dpad(const char* channel) const {
    return value(channel) != 0.0f;
}
