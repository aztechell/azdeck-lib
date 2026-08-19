#include "BleTransport.h"

#include "../PlatformCaps.h"

#if AZDECK_USE_ARDUINOBLE
#include "UnoR4BleBackend.h"
#elif defined(ESP32) && AZDECK_HAS_BLE
#include "Esp32BleBackend.h"
#endif

bool azdeckBleBegin(const char* deviceName) {
#if AZDECK_USE_ARDUINOBLE
    return azdeckUnoR4BleStart(deviceName);
#elif defined(ESP32) && AZDECK_HAS_BLE
    return azdeckEsp32BleStart(deviceName);
#else
    (void)deviceName;
    return false;
#endif
}

void azdeckBleUpdate() {
#if AZDECK_USE_ARDUINOBLE
    azdeckUnoR4BlePoll();
#elif defined(ESP32) && AZDECK_HAS_BLE
    azdeckEsp32BlePoll();
#endif
}

bool azdeckBleTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
) {
#if AZDECK_USE_ARDUINOBLE
    return azdeckUnoR4BleTake(buffer, capacity, outLength, overflow);
#elif defined(ESP32) && AZDECK_HAS_BLE
    return azdeckEsp32BleTake(buffer, capacity, outLength, overflow);
#else
    (void)buffer;
    (void)capacity;
    (void)outLength;
    if (overflow != nullptr) {
        *overflow = false;
    }
    return false;
#endif
}

bool azdeckBleTakeDisconnect() {
#if AZDECK_USE_ARDUINOBLE
    return azdeckUnoR4BleTakeDisconnect();
#elif defined(ESP32) && AZDECK_HAS_BLE
    return azdeckEsp32BleTakeDisconnect();
#else
    return false;
#endif
}

bool azdeckBleConnected() {
#if AZDECK_USE_ARDUINOBLE
    return azdeckUnoR4BleConnected();
#elif defined(ESP32) && AZDECK_HAS_BLE
    return azdeckEsp32BleConnected();
#else
    return false;
#endif
}

void azdeckBleSend(const char* data, size_t length) {
#if AZDECK_USE_ARDUINOBLE
    azdeckUnoR4BleSend(data, length);
#elif defined(ESP32) && AZDECK_HAS_BLE
    azdeckEsp32BleSend(data, length);
#else
    (void)data;
    (void)length;
#endif
}
