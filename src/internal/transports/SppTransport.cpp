#include "SppTransport.h"

#include "../Inbox.h"
#include "../PlatformCaps.h"

#if AZDECK_HAS_SPP
#include <BluetoothSerial.h>
#endif

#if AZDECK_HAS_SPP

namespace {
BluetoothSerial gSerialBt;
AzDeckInbox gInbox;
char gLine[AZDECK_RX_BUFFER_SIZE];
size_t gLineLength = 0;
bool gHadClient = false;
bool gStarted = false;
}  // namespace

bool azdeckSppBegin(const char* deviceName) {
    azdeckInboxInit(gInbox);
    gLineLength = 0;
    gHadClient = false;
    gStarted = gSerialBt.begin(deviceName != nullptr ? deviceName : "AzDeck");
    return gStarted;
}

void azdeckSppUpdate() {
    if (!gStarted) {
        return;
    }

    const bool connected = gSerialBt.hasClient();
    if (gHadClient && !connected) {
        gInbox.disconnect = true;
        gLineLength = 0;
    }
    gHadClient = connected;

    while (gSerialBt.available() > 0) {
        const char current = static_cast<char>(gSerialBt.read());
        if (current == '\r') {
            continue;
        }
        if (current == '\n') {
            if (gLineLength > 0) {
                azdeckInboxPush(
                    gInbox,
                    reinterpret_cast<const uint8_t*>(gLine),
                    gLineLength
                );
                gLineLength = 0;
            }
            continue;
        }
        if (gLineLength + 1 >= AZDECK_RX_BUFFER_SIZE) {
            gInbox.overflow = true;
            gLineLength = 0;
            continue;
        }
        gLine[gLineLength++] = current;
    }
}

bool azdeckSppTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
) {
    return azdeckInboxTake(gInbox, buffer, capacity, outLength, nullptr, overflow);
}

bool azdeckSppTakeDisconnect() {
    return azdeckInboxTakeDisconnect(gInbox);
}

void azdeckSppSend(const char* data, size_t length) {
    if (!gStarted || data == nullptr || length == 0) {
        return;
    }
    gSerialBt.write(reinterpret_cast<const uint8_t*>(data), length);
    gSerialBt.write('\n');
}

#else

bool azdeckSppBegin(const char* deviceName) {
    (void)deviceName;
    return false;
}

void azdeckSppUpdate() {}

bool azdeckSppTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
) {
    (void)buffer;
    (void)capacity;
    (void)outLength;
    if (overflow != nullptr) {
        *overflow = false;
    }
    return false;
}

bool azdeckSppTakeDisconnect() {
    return false;
}

void azdeckSppSend(const char* data, size_t length) {
    (void)data;
    (void)length;
}

#endif
