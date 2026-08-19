# AzDeck

Controller app: [AzDeck on Google Play](https://play.google.com/store/apps/details?id=com.aztechell.azdeck)

```cpp
#include <AzDeck.h>

AzDeck deck;

void setup() {
    deck.begin(BLE, JSON);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
}
```

Arduino library for receiving named control channels from the AzDeck Android app. The sketch does not set up BLE, TCP, WebSocket, or Bluetooth SPP, and does not parse JSON or TEXT.

App, firmware, and protocol: [aztechell/azdeck](https://github.com/aztechell/azdeck)

## Install

1. Arduino IDE 2.x
2. Sketch → Include Library → Add .ZIP Library, or clone this repo into `Arduino/libraries/AzDeck`
3. Install **ArduinoJson 6.x** (not 7) and **WebSockets** by Markus Sattler when prompted

For Arduino UNO R4 WiFi BLE, also install **ArduinoBLE**.

## Reading controls

```cpp
deck.update();

float x = deck.axis("move_x");
float y = deck.axis("move_y");
float speed = deck.slider("speed");
bool l2 = deck.button("l2");
bool up = deck.dpad("up");
float custom = deck.value("custom_channel");
```

Channel names come from the AzDeck profile. Unknown channels return `0`.

Timeout defaults to **350 ms**. `deck.begin(BLE, JSON, 500)` changes it. Passing `0` still uses 350 ms.

If no valid control packet arrives within the timeout, every stored channel is set to `0`.

## Optional transport setup

BLE / Bluetooth SPP device name (default `AzDeck`):

```cpp
deck.name("My Robot");
deck.begin(BLE, JSON);
```

Wi-Fi in v0.1.0 always creates a **controller Access Point** (not Station Mode), then starts a server:

```cpp
deck.wifi("My Robot", "12345678", 81);
deck.begin(WEBSOCKET, JSON);
```

```cpp
deck.wifi("My Robot", "12345678", 5000);
deck.begin(TCP, JSON);
```

Defaults if `wifi()` is not called:

| | WebSocket | TCP |
|---|---|---|
| SSID | `AzDeck` | `AzDeck` |
| Password | `azdeck123` | `azdeck123` |
| IP | `192.168.4.1` | `192.168.4.1` |
| Port | `81` | `5000` |

## Transports and boards

```cpp
deck.begin(BLE, JSON);
deck.begin(WEBSOCKET, JSON);
deck.begin(TCP, JSON);
deck.begin(SPP, TEXT);
```

| Board | BLE | WebSocket | TCP | SPP |
|---|:---:|:---:|:---:|:---:|
| ESP32 classic | yes | yes | yes | yes |
| ESP32-S2 | no | yes | yes | no |
| ESP32-S3 | yes | yes | yes | no |
| ESP32-C3 / C5 / C6 | yes | yes | yes | no |
| ESP32-H2 | yes | no | no | no |
| ESP8266 | no | yes | yes | no |
| Arduino UNO R4 WiFi | yes | yes | yes | no |

Unsupported combinations compile, and `begin()` returns `false`. The library does not print to Serial.

ESP32 classic sketches that include AzDeck also pull BLE, Classic Bluetooth, Wi-Fi, and WebSockets. With Arduino-ESP32 3.3.x that binary is about **1.67 MB**. It does **not** fit the default **1.31 MB** app partition (`BLE_Minimal` and `WebSocket_Minimal` both measured at 127% of 1,310,720 bytes). On a 4 MB module choose **Tools → Partition Scheme → Huge APP** (or another scheme with at least 2 MB for the app). ESP32-S3 and ESP32-C3 `BLE_Minimal` sketches fit the default partition.

## Channel names and packet size

Maximum channel name length is **32** characters. Longer keys are ignored; they are never truncated. The store holds up to **32** channels.

Incoming packets are limited to **512** bytes on ESP8266 and UNO R4 WiFi, and **1024** bytes on ESP32. A snapshot that does not fit is discarded and all channels go to `0`.

Typical AzDeck keys such as `move_x` fit 32 JSON channels in those packet limits. Thirty-two channels with 32-character names need more than 512 bytes and will not fit on ESP8266 / UNO R4 WiFi.

## Serializers

JSON (recommended) and TEXT (`key:value` or `key=value`, separated by space, `;`, or `,`).

## Limits for v0.1.0

- Access Point only; no router / Station Mode
- No UDP, telemetry, labels, or robot/motor APIs
- ArduinoJson 6.x only (`StaticJsonDocument`)
