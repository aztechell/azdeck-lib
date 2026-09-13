#pragma once

#include "../AzDeckTypes.h"
#include "ChannelStore.h"
#include "Config.h"
#include "TelemetryStore.h"

#include <stddef.h>
#include <stdint.h>

class AzDeckControlCore {
public:
    AzDeckControlCore();

    void configure(
        AzDeckTransport transport,
        AzDeckSerializer serializer,
        uint16_t timeoutMs
    );

    void handlePayload(char* data, size_t length, uint32_t nowMs);
    void checkTimeout(uint32_t nowMs);
    void failsafe();
    void noteDisconnect();

    float value(const char* channel) const;
    uint16_t timeoutMs() const;
    bool hasCommand() const;
    bool takePong(char* buffer, size_t capacity, size_t* outLength);
    bool queueTelemetry(const char* channel, float value);
    bool queueTelemetry(const char* channel, const char* text);
    bool takeTelemetry(char* buffer, size_t capacity, size_t* outLength);

private:
    AzDeckChannelStore channels_;
    AzDeckTelemetryStore telemetry_;
    AzDeckTransport transport_;
    AzDeckSerializer serializer_;
    uint16_t timeoutMs_;
    uint32_t lastCommandMs_;
    bool hasCommand_;
    char pong_[AZDECK_RX_BUFFER_SIZE];
    size_t pongLength_;
};
