#include "../PlatformCaps.h"

#if AZDECK_USE_ARDUINOBLE

#include "UnoR4BleBackend.h"

#include "../Inbox.h"
#include "../NusUuids.h"

#include <ArduinoBLE.h>

namespace {
BLEService gService(AZDECK_NUS_SERVICE_UUID);
BLECharacteristic gRx(
    AZDECK_NUS_WRITE_CHARACTERISTIC,
    BLEWrite | BLEWriteWithoutResponse,
    AZDECK_RX_BUFFER_SIZE - 1
);
BLECharacteristic gTx(
    AZDECK_NUS_NOTIFY_CHARACTERISTIC,
    BLENotify,
    AZDECK_RX_BUFFER_SIZE - 1
);
AzDeckInbox gInbox;
bool gWasConnected = false;

void onRxWritten(BLEDevice central, BLECharacteristic characteristic) {
    (void)central;
    azdeckInboxPush(
        gInbox,
        characteristic.value(),
        static_cast<size_t>(characteristic.valueLength())
    );
}
}  // namespace

bool azdeckUnoR4BleStart(const char* deviceName) {
    azdeckInboxInit(gInbox);
    gWasConnected = false;

    if (!BLE.begin()) {
        return false;
    }

    const char* name = (deviceName != nullptr && deviceName[0] != '\0')
        ? deviceName
        : "AzDeck";
    BLE.setLocalName(name);
    BLE.setDeviceName(name);
    BLE.setAdvertisedService(gService);
    gService.addCharacteristic(gRx);
    gService.addCharacteristic(gTx);
    BLE.addService(gService);
    gRx.setEventHandler(BLEWritten, onRxWritten);
    BLE.advertise();
    return true;
}

void azdeckUnoR4BlePoll() {
    BLE.poll();
    const bool connected = BLE.connected();
    if (gWasConnected && !connected) {
        gInbox.disconnect = true;
    }
    gWasConnected = connected;
}

bool azdeckUnoR4BleTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
) {
    return azdeckInboxTake(gInbox, buffer, capacity, outLength, nullptr, overflow);
}

bool azdeckUnoR4BleTakeDisconnect() {
    return azdeckInboxTakeDisconnect(gInbox);
}

bool azdeckUnoR4BleConnected() {
    return BLE.connected();
}

void azdeckUnoR4BleSend(const char* data, size_t length) {
    if (data == nullptr || length == 0 || !BLE.connected()) {
        return;
    }
    gTx.writeValue(data, length);
}

#endif
