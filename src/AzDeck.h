#pragma once

#include <Arduino.h>
#include "AzDeckTypes.h"
#include "internal/Config.h"
#include "internal/ControlCore.h"

class AzDeck {
public:
    AzDeck();

    void name(const char* deviceName);

    void wifi(
        const char* ssid,
        const char* password,
        uint16_t port
    );

    void settings(const char* html);
    void settings(const char* html, void (*onQuery)(const char* query));

    bool begin(
        AzDeckTransport transport,
        uint16_t timeoutMs = 350
    );

    void update();

    float value(const char* channel) const;
    float axis(const char* channel) const;
    float slider(const char* channel) const;
    bool button(const char* channel) const;
    bool dpad(const char* channel) const;
    void send(const char* channel, float value);
    void send(const char* channel, const char* text);

private:
    void failsafe();
    void handlePayload(char* data, size_t length, uint8_t clientId);
    void sendReply(const char* data, size_t length, uint8_t clientId);
    void checkTimeout();
    bool startTransport();
    void pollTransport();

    AzDeckControlCore core_;
    AzDeckTransport transport_;
    bool started_;
    bool wifiConfigured_;
    char deviceName_[AZDECK_MAX_DEVICE_NAME_LENGTH + 1];
    char ssid_[AZDECK_MAX_SSID_LENGTH + 1];
    char password_[AZDECK_MAX_PASSWORD_LENGTH + 1];
    uint16_t netPort_;
    const char* settingsHtml_;
    void (*settingsQuery_)(const char* query);
};
