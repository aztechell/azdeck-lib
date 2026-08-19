#include "../PlatformCaps.h"

#if defined(ESP32) && AZDECK_HAS_BLE && !AZDECK_USE_ARDUINOBLE

#include "Esp32BleBackend.h"

#include "../Config.h"
#include "../NusUuids.h"
#include "../PacketQueue.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <string.h>
#if defined(__has_include)
#if __has_include(<BLE2902.h>)
#include <BLE2902.h>
#define AZDECK_HAS_BLE2902 1
#endif
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace {
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
AzDeckPacketQueue gQueue;
volatile bool gConnected = false;

BLEServer* gServer = nullptr;
BLECharacteristic* gNotify = nullptr;

void queueReset() {
    portENTER_CRITICAL(&gMux);
    gQueue.reset();
    portEXIT_CRITICAL(&gMux);
}

void queuePush(const uint8_t* data, size_t length) {
    portENTER_CRITICAL(&gMux);
    gQueue.push(data, length);
    portEXIT_CRITICAL(&gMux);
}

bool queueTake(char* buffer, size_t capacity, size_t* outLength, bool* overflow) {
    bool overflowed = false;
    portENTER_CRITICAL(&gMux);
    const bool ok = gQueue.take(buffer, capacity, outLength, nullptr, &overflowed);
    portEXIT_CRITICAL(&gMux);
    if (overflow != nullptr) {
        *overflow = overflowed;
    }
    return ok;
}

bool queueTakeDisconnect() {
    portENTER_CRITICAL(&gMux);
    const bool disconnected = gQueue.takeDisconnect();
    portEXIT_CRITICAL(&gMux);
    return disconnected;
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        (void)server;
        gConnected = true;
    }

    void onDisconnect(BLEServer* server) override {
        (void)server;
        gConnected = false;
        portENTER_CRITICAL(&gMux);
        gQueue.markDisconnect();
        portEXIT_CRITICAL(&gMux);
        BLEDevice::startAdvertising();
    }
};

class WriteCallbacks : public BLECharacteristicCallbacks {
    void handleWrite(BLECharacteristic* characteristic) {
        if (characteristic == nullptr) {
            return;
        }
        auto raw = characteristic->getValue();
        queuePush(
            reinterpret_cast<const uint8_t*>(raw.c_str()),
            raw.length()
        );
    }

    void onWrite(BLECharacteristic* characteristic) override {
        handleWrite(characteristic);
    }

#if defined(CONFIG_BLUEDROID_ENABLED)
    void onWrite(BLECharacteristic* characteristic, esp_ble_gatts_cb_param_t* param) override {
        (void)param;
        handleWrite(characteristic);
    }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
    void onWrite(BLECharacteristic* characteristic, ble_gap_conn_desc* desc) override {
        (void)desc;
        handleWrite(characteristic);
    }
#endif
};

ServerCallbacks gServerCallbacks;
WriteCallbacks gWriteCallbacks;
}  // namespace

bool azdeckEsp32BleStart(const char* deviceName) {
    queueReset();
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
    return queueTake(buffer, capacity, outLength, overflow);
}

bool azdeckEsp32BleTakeDisconnect() {
    return queueTakeDisconnect();
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
