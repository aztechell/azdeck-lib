#include "../PlatformCaps.h"

#if defined(ESP32) && AZDECK_HAS_BLE && !AZDECK_USE_ARDUINOBLE

#include "Esp32BleBackend.h"

#include "../Inbox.h"
#include "../NusUuids.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#if defined(__has_include)
#if __has_include(<BLE2902.h>)
#include <BLE2902.h>
#define AZDECK_HAS_BLE2902 1
#endif
#endif

namespace {
AzDeckInbox gInbox;
BLEServer* gServer = nullptr;
BLECharacteristic* gNotify = nullptr;
volatile bool gConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        (void)server;
        gConnected = true;
    }

    void onDisconnect(BLEServer* server) override {
        (void)server;
        gConnected = false;
        gInbox.disconnect = true;
        BLEDevice::startAdvertising();
    }
};

class WriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        if (characteristic == nullptr) {
            return;
        }
        auto raw = characteristic->getValue();
        azdeckInboxPush(
            gInbox,
            reinterpret_cast<const uint8_t*>(raw.c_str()),
            raw.length()
        );
    }
};

ServerCallbacks gServerCallbacks;
WriteCallbacks gWriteCallbacks;
}  // namespace

bool azdeckEsp32BleStart(const char* deviceName) {
    azdeckInboxInit(gInbox);
    gConnected = false;
    gNotify = nullptr;

    const char* name = (deviceName != nullptr && deviceName[0] != '\0')
        ? deviceName
        : "AzDeck";
    BLEDevice::init(name);
    gServer = BLEDevice::createServer();
    if (gServer == nullptr) {
        return false;
    }
    gServer->setCallbacks(&gServerCallbacks);

    BLEService* service = gServer->createService(AZDECK_NUS_SERVICE_UUID);
    if (service == nullptr) {
        return false;
    }

    BLECharacteristic* writeCharacteristic = service->createCharacteristic(
        AZDECK_NUS_WRITE_CHARACTERISTIC,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    gNotify = service->createCharacteristic(
        AZDECK_NUS_NOTIFY_CHARACTERISTIC,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    if (writeCharacteristic == nullptr || gNotify == nullptr) {
        return false;
    }

    writeCharacteristic->setCallbacks(&gWriteCallbacks);
#if defined(AZDECK_HAS_BLE2902)
    gNotify->addDescriptor(new BLE2902());
#endif

    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(AZDECK_NUS_SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    return true;
}

void azdeckEsp32BlePoll() {}

bool azdeckEsp32BleTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
) {
    return azdeckInboxTake(gInbox, buffer, capacity, outLength, nullptr, overflow);
}

bool azdeckEsp32BleTakeDisconnect() {
    return azdeckInboxTakeDisconnect(gInbox);
}

bool azdeckEsp32BleConnected() {
    return gConnected;
}

void azdeckEsp32BleSend(const char* data, size_t length) {
    if (gNotify == nullptr || !gConnected || data == nullptr || length == 0) {
        return;
    }
    gNotify->setValue(reinterpret_cast<uint8_t*>(const_cast<char*>(data)), length);
    gNotify->notify();
}

#endif
