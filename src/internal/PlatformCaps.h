#pragma once

#if defined(ESP32)
#if defined(__has_include)
#if __has_include("soc/soc_caps.h")
#include "soc/soc_caps.h"
#endif
#endif
#endif

#ifndef AZDECK_HAS_WIFI
#if defined(ESP8266)
#define AZDECK_HAS_WIFI 1
#elif defined(ARDUINO_UNOR4_WIFI)
#define AZDECK_HAS_WIFI 1
#elif defined(ESP32) && defined(SOC_WIFI_SUPPORTED)
#define AZDECK_HAS_WIFI SOC_WIFI_SUPPORTED
#elif defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32H2)
#define AZDECK_HAS_WIFI 1
#else
#define AZDECK_HAS_WIFI 0
#endif
#endif

#ifndef AZDECK_HAS_BLE
#if defined(ARDUINO_UNOR4_WIFI)
#define AZDECK_HAS_BLE 1
#elif defined(ESP32) && defined(SOC_BLE_SUPPORTED)
#define AZDECK_HAS_BLE SOC_BLE_SUPPORTED
#elif defined(ESP32) && defined(CONFIG_IDF_TARGET_ESP32S2)
#define AZDECK_HAS_BLE 0
#elif defined(ESP32)
#define AZDECK_HAS_BLE 1
#else
#define AZDECK_HAS_BLE 0
#endif
#endif

#ifndef AZDECK_HAS_SPP
#if defined(ESP32) && defined(SOC_BT_CLASSIC_SUPPORTED)
#define AZDECK_HAS_SPP SOC_BT_CLASSIC_SUPPORTED
#elif defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2) && \
    !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C2) && \
    !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C5) && \
    !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
#define AZDECK_HAS_SPP 1
#else
#define AZDECK_HAS_SPP 0
#endif
#endif

#ifndef AZDECK_USE_ARDUINOBLE
#if defined(ARDUINO_UNOR4_WIFI)
#define AZDECK_USE_ARDUINOBLE 1
#else
#define AZDECK_USE_ARDUINOBLE 0
#endif
#endif
