# AzDeck

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

Arduino library for receiving named control channels from the [AzDeck](https://github.com/aztechell/azdeck) app. The sketch does not set up BLE, TCP, WebSocket, or Bluetooth SPP, and does not parse JSON or TEXT.

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

Optional timeout (default 350 ms):

```cpp
deck.begin(BLE, JSON, 500);
```

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

## Channel names

Maximum channel name length is **32** characters (`AZDECK_MAX_CHANNEL_NAME_LENGTH`). Longer keys in a packet are ignored; they are never truncated. Up to **32** channels are stored. Extra keys are ignored.

## Serializers

JSON (recommended) and TEXT (`key:value` or `key=value`, separated by space, `;`, or `,`).

## Limits for v0.1.0

- Access Point only; no router / Station Mode
- No UDP, telemetry, labels, or robot/motor APIs
- ArduinoJson 6.x only (`StaticJsonDocument`)
