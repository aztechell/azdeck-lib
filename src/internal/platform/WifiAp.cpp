#include "WifiAp.h"

#include "../PlatformCaps.h"

#if AZDECK_HAS_WIFI
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_UNOR4_WIFI)
#include <WiFiS3.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#endif

bool azdeckStartAccessPoint(const char* ssid, const char* password) {
#if !AZDECK_HAS_WIFI
    (void)ssid;
    (void)password;
    return false;
#elif defined(ARDUINO_UNOR4_WIFI)
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }
    const int status = (password != nullptr && password[0] != '\0')
        ? WiFi.beginAP(ssid, password)
        : WiFi.beginAP(ssid);
    return status == WL_AP_LISTENING || status == WL_AP_CONNECTED ||
           status == WL_CONNECTED;
#elif defined(ESP8266)
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }
    WiFi.mode(WIFI_AP);
    WiFi.persistent(false);
    const IPAddress ip(192, 168, 4, 1);
    const IPAddress gateway(192, 168, 4, 1);
    const IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(ip, gateway, subnet);
    return WiFi.softAP(ssid, password);
#elif defined(ESP32)
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }
    WiFi.mode(WIFI_AP);
    return WiFi.softAP(ssid, password);
#else
    (void)ssid;
    (void)password;
    return false;
#endif
}
