#include "ControlCore.h"

#include "../AzDeckTypes.h"
#include "JsonParser.h"
#include "Ping.h"
#include "TextParser.h"

#include <string.h>

AzDeckControlCore::AzDeckControlCore()
    : transport_(BLE),
      serializer_(JSON),
      timeoutMs_(AZDECK_DEFAULT_TIMEOUT_MS),
      lastCommandMs_(0),
      hasCommand_(false),
      pongLength_(0) {
    pong_[0] = '\0';
}

void AzDeckControlCore::configure(
    AzDeckTransport transport,
    AzDeckSerializer serializer,
    uint16_t timeoutMs
) {
    transport_ = transport;
    serializer_ = serializer;
    timeoutMs_ = (timeoutMs == 0) ? AZDECK_DEFAULT_TIMEOUT_MS : timeoutMs;
    lastCommandMs_ = 0;
    hasCommand_ = false;
    pongLength_ = 0;
    channels_.zeroValues();
}

void AzDeckControlCore::failsafe() {
    channels_.zeroValues();
}

void AzDeckControlCore::noteDisconnect() {
    failsafe();
    hasCommand_ = false;
}

void AzDeckControlCore::handlePayload(char* data, size_t length, uint32_t nowMs) {
    pongLength_ = 0;
    if (data == nullptr || length == 0) {
        return;
    }

    if (azdeckIsPing(data, length)) {
        if (azdeckBuildPong(data, length, pong_, sizeof(pong_), &pongLength_)) {
            return;
        }
        pongLength_ = 0;
        return;
    }

    if (serializer_ == JSON) {
        const AzDeckJsonParseResult result = azdeckParseJson(data, length, channels_);
        if (result == AZDECK_JSON_MALFORMED) {
            failsafe();
            return;
        }
        if (result == AZDECK_JSON_SERVICE || result == AZDECK_JSON_EMPTY) {
            return;
        }
        lastCommandMs_ = nowMs;
        hasCommand_ = true;
        return;
    }

    const AzDeckTextParseResult result = azdeckParseText(data, length, channels_);
    if (result == AZDECK_TEXT_MALFORMED) {
        failsafe();
        return;
    }
    if (result == AZDECK_TEXT_EMPTY) {
        return;
    }
    lastCommandMs_ = nowMs;
    hasCommand_ = true;
}

void AzDeckControlCore::checkTimeout(uint32_t nowMs) {
    if (!hasCommand_) {
        return;
    }
    if (nowMs - lastCommandMs_ > timeoutMs_) {
        failsafe();
        hasCommand_ = false;
    }
}

float AzDeckControlCore::value(const char* channel) const {
    return channels_.get(channel);
}

uint16_t AzDeckControlCore::timeoutMs() const {
    return timeoutMs_;
}

bool AzDeckControlCore::hasCommand() const {
    return hasCommand_;
}

bool AzDeckControlCore::takePong(char* buffer, size_t capacity, size_t* outLength) {
    if (pongLength_ == 0 || buffer == nullptr || capacity == 0) {
        return false;
    }
    size_t n = pongLength_;
    if (n >= capacity) {
        n = capacity - 1;
    }
    memcpy(buffer, pong_, n);
    buffer[n] = '\0';
    if (outLength != nullptr) {
        *outLength = n;
    }
    pongLength_ = 0;
    return true;
}
