#pragma once

#include "Config.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct AzDeckInbox {
    char data[AZDECK_RX_BUFFER_SIZE];
    size_t len;
    volatile bool ready;
    volatile bool overflow;
    volatile bool disconnect;
    uint8_t clientId;
};

inline void azdeckInboxInit(AzDeckInbox& inbox) {
    inbox.len = 0;
    inbox.ready = false;
    inbox.overflow = false;
    inbox.disconnect = false;
    inbox.clientId = 0;
    inbox.data[0] = '\0';
}

inline void azdeckInboxPush(
    AzDeckInbox& inbox,
    const uint8_t* data,
    size_t length,
    uint8_t clientId = 0
) {
    if (data == nullptr || length == 0) {
        return;
    }
    if (length >= AZDECK_RX_BUFFER_SIZE) {
        inbox.overflow = true;
        inbox.ready = false;
        return;
    }
    memcpy(inbox.data, data, length);
    inbox.data[length] = '\0';
    inbox.len = length;
    inbox.clientId = clientId;
    inbox.ready = true;
}

inline bool azdeckInboxTake(
    AzDeckInbox& inbox,
    char* buffer,
    size_t capacity,
    size_t* outLength,
    uint8_t* clientId,
    bool* overflow
) {
    if (inbox.overflow) {
        if (overflow != nullptr) {
            *overflow = true;
        }
        inbox.overflow = false;
        inbox.ready = false;
        return false;
    }
    if (overflow != nullptr) {
        *overflow = false;
    }
    if (!inbox.ready) {
        return false;
    }

    size_t n = inbox.len;
    if (capacity == 0) {
        inbox.ready = false;
        return false;
    }
    if (n >= capacity) {
        n = capacity - 1;
    }
    memcpy(buffer, inbox.data, n);
    buffer[n] = '\0';
    if (outLength != nullptr) {
        *outLength = n;
    }
    if (clientId != nullptr) {
        *clientId = inbox.clientId;
    }
    inbox.ready = false;
    return true;
}

inline bool azdeckInboxTakeDisconnect(AzDeckInbox& inbox) {
    if (!inbox.disconnect) {
        return false;
    }
    inbox.disconnect = false;
    return true;
}
