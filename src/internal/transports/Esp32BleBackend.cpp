#include "../PlatformCaps.h"

#if defined(ESP32) && AZDECK_HAS_BLE && !AZDECK_USE_ARDUINOBLE

#include "Esp32BleBackend.h"

#include "../Config.h"
#include "../NusUuids.h"

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
struct PacketSlot {
    char data[AZDECK_RX_BUFFER_SIZE];
    size_t len;
};

portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
PacketSlot gSlots[AZDECK_PACKET_QUEUE_DEPTH];
uint8_t gHead = 0;
uint8_t gTail = 0;
uint8_t gCount = 0;
volatile bool gOverflow = false;
volatile bool gDisconnect = false;
volatile bool gConnected = false;

BLEServer* gServer = nullptr;
BLECharacteristic* gNotify = nullptr;

void queueReset() {
    portENTER_CRITICAL(&gMux);
    gHead = 0;
    gTail = 0;
    gCount = 0;
    gOverflow = false;
    gDisconnect = false;
    portEXIT_CRITICAL(&gMux);
}

void queuePush(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return;
    }

    portENTER_CRITICAL(&gMux);
    if (length >= AZDECK_RX_BUFFER_SIZE) {
        gOverflow = true;
        portEXIT_CRITICAL(&gMux);
        return;
    }

    if (gCount >= AZDECK_PACKET_QUEUE_DEPTH) {
        gTail = static_cast<uint8_t>((gTail + 1) % AZDECK_PACKET_QUEUE_DEPTH);
        gCount--;
    }

    memcpy(gSlots[gHead].data, data, length);
    gSlots[gHead].data[length] = '\0';
    gSlots[gHead].len = length;
    gHead = static_cast<uint8_t>((gHead + 1) % AZDECK_PACKET_QUEUE_DEPTH);
    gCount++;
    portEXIT_CRITICAL(&gMux);
}

bool queueTake(char* buffer, size_t capacity, size_t* outLength, bool* overflow) {
    portENTER_CRITICAL(&gMux);
    const bool overflowed = gOverflow;
    gOverflow = false;
    if (overflowed) {
        portEXIT_CRITICAL(&gMux);
        if (overflow != nullptr) {
            *overflow = true;
        }
        return false;
    }
    if (gCount == 0) {
        portEXIT_CRITICAL(&gMux);
        if (overflow != nullptr) {
            *overflow = false;
        }
        return false;
    }

    const PacketSlot& slot = gSlots[gTail];
    size_t n = slot.len;
    if (capacity == 0) {
        gTail = static_cast<uint8_t>((gTail + 1) % AZDECK_PACKET_QUEUE_DEPTH);
        gCount--;
        portEXIT_CRITICAL(&gMux);
        return false;
    }
    if (n >= capacity) {
        n = capacity - 1;
    }
    memcpy(buffer, slot.data, n);
    gTail = static_cast<uint8_t>((gTail + 1) % AZDECK_PACKET_QUEUE_DEPTH);
    gCount--;
    portEXIT_CRITICAL(&gMux);

    buffer[n] = '\0';
    if (outLength != nullptr) {
        *outLength = n;
    }
    if (overflow != nullptr) {
        *overflow = false;
    }
    return true;
}

bool queueTakeDisconnect() {
    portENTER_CRITICAL(&gMux);
    const bool disconnected = gDisconnect;
    gDisconnect = false;
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
        gDisconnect = true;
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
