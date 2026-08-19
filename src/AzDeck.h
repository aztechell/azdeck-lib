#pragma once

#include <Arduino.h>
#include "internal/ChannelStore.h"

enum AzDeckTransport {
    BLE,
    TCP,
    WEBSOCKET,
    SPP
};

enum AzDeckSerializer {
    JSON,
    TEXT
};

class AzDeck {
public:
    AzDeck();

    void name(const char* deviceName);

    void wifi(
        const char* ssid,
        const char* password,
        uint16_t port
    );

    bool begin(
        AzDeckTransport transport,
        AzDeckSerializer serializer,
        uint16_t timeoutMs = 350
    );

    void update();

    float value(const char* channel) const;
    float axis(const char* channel) const;
    float slider(const char* channel) const;
    bool button(const char* channel) const;
    bool dpad(const char* channel) const;

private:
    void failsafe();
    void handlePayload(const char* data, size_t length, uint8_t clientId);
    void sendReply(const char* data, size_t length, uint8_t clientId);
    void checkTimeout();
    bool startTransport();
    void pollTransport();

    static void tcpPayloadThunk(void* context, const char* data, size_t length);
    static void tcpFailThunk(void* context);
    static void tcpDisconnectThunk(void* context);

    AzDeckChannelStore channels_;
    AzDeckTransport transport_;
    AzDeckSerializer serializer_;
    uint16_t timeoutMs_;
    uint32_t lastCommandMs_;
    bool started_;
    bool hasCommand_;
    bool wifiConfigured_;
    char deviceName_[AZDECK_MAX_DEVICE_NAME_LENGTH + 1];
    char ssid_[AZDECK_MAX_SSID_LENGTH + 1];
    char password_[AZDECK_MAX_PASSWORD_LENGTH + 1];
    uint16_t netPort_;
};
