# AzDeck

Controller app: [AzDeck on Google Play](https://play.google.com/store/apps/details?id=com.aztechell.azdeck)

```cpp
#include <AzDeck.h>

AzDeck deck;

void setup() {
    deck.begin(BLE);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
}
```

Arduino library for receiving named control channels from the AzDeck Android app. The sketch does not set up BLE, WebSocket, or Bluetooth SPP, and does not parse JSON.

App, firmware, and protocol: [aztechell/azdeck](https://github.com/aztechell/azdeck)

## Install

1. Arduino IDE 2.x
2. Sketch → Include Library → Add .ZIP Library, or clone this repo into `Arduino/libraries/AzDeck`
3. Install **ArduinoJson 6.x** (not 7) and **WebSockets** by Markus Sattler when prompted

ArduinoBLE is installed automatically as an AzDeck dependency and is used by the UNO R4 WiFi BLE backend.

## Matrix Mini R4

`examples/MatrixR4_Drive` drives motors M1/M2 from a D-pad and `speed`, and servos RC3/RC4 from `ser1` / `ser2`. Install **MatrixMiniR4**. Import `examples/MatrixR4_Drive/profile/Matrix_R4.profile.json` in the AzDeck app, then pair BLE.

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

Timeout defaults to **350 ms**. `deck.begin(BLE, 500)` changes it. Passing `0` still uses 350 ms.

If no valid control packet arrives within the timeout, every stored channel is set to `0`.

Control snapshots are JSON objects with named numeric channels, for example `{"move_x":1,"l2":1}`. A JSON object with a `"type"` field is ignored (no failsafe, no timeout refresh).

The app may send `AZDECK_PING:` plus a short hex token. The library replies `AZDECK_PONG:` with the same token on BLE notify or WebSocket (and SPP with a newline). Ping does not change channels or the timeout.

## Sending telemetry

`send()` queues a number or a short string (max **80** characters, or **32** on UNO R4 WiFi). `update()` flushes dirty keys as one JSON object `{"type":"telemetry",...}` on the same reply path as ping (BLE notify, WebSocket, or SPP). BLE profiles need a notify characteristic.

```cpp
deck.send("battery", 7.4f);
deck.send("serial", "hello");
deck.update();
```

Call `send` on a timer or when a value changes (about 100–250 ms). Sending a new value every `loop()` will flood BLE, especially on UNO R4 WiFi. UNO R4 WiFi keeps **8** telemetry keys and **32**-character strings so Matrix Mini R4 sketches fit in RAM.

In the AzDeck app, bind a telemetry label to the same channel name. `examples/Telemetry_Minimal` sends `count` 0–1000 every 200 ms over WebSocket (`ws://192.168.4.1:81`, Wi-Fi `AzDeck` / `azdeck123`).

## Optional transport setup

BLE / Bluetooth SPP device name (default `AzDeck`):

```cpp
deck.name("My Robot");
deck.begin(BLE);
```

Wi-Fi in v0.2.1 always creates a **controller Access Point** (not Station Mode), then starts a WebSocket server:

```cpp
deck.wifi("My Robot", "12345678", 81);
deck.begin(WEBSOCKET);
```

Defaults if `wifi()` is not called:

| | WebSocket |
|---|---|
| SSID | `AzDeck` |
| Password | `azdeck123` |
| IP | `192.168.4.1` |
| Port | `81` |

## Device settings page

WebSocket only. Call `settings()` **before** `begin` with HTML the library serves at `http://192.168.4.1/settings` (HTTP port 80). The app probes with `HEAD` and needs `X-AzDeck-Settings: 1`. BLE and SPP do not use this.

```cpp
deck.settings(html, onQuery);
deck.begin(WEBSOCKET);
```

`GET /settings?led=1` still serves the page and calls `onQuery("led=1")`. `examples/Settings_Minimal` toggles `LED_BUILTIN`.

If `settings()` is not called, port 80 stays closed and the app Settings button stays off. Opening Settings sends the profile's initial channel values, then pauses control packets; the 350 ms failsafe still zeros channels.

## Transports and boards

```cpp
deck.begin(BLE);
deck.begin(WEBSOCKET);
deck.begin(SPP);
```

| Board | BLE | WebSocket | SPP |
|---|:---:|:---:|:---:|
| ESP32 classic | yes | yes | yes |
| ESP32-S2 | no | yes | no |
| ESP32-S3 | yes | yes | no |
| ESP32-C3 / C5 / C6 | yes | yes | no |
| ESP32-H2 | yes | no | no |
| ESP8266 | no | yes | no |
| Arduino UNO R4 WiFi | yes | yes | no |

Unsupported combinations compile, and `begin()` returns `false`. The library does not print to Serial.

ESP32 classic sketches that include AzDeck also pull BLE, Classic Bluetooth, Wi-Fi, and WebSockets. With Arduino-ESP32 3.3.x that binary is about **1.67 MB**. It does **not** fit the default **1.31 MB** app partition (`BLE_Minimal` and `WebSocket_Minimal` both measured at 127% of 1,310,720 bytes). On a 4 MB module choose **Tools → Partition Scheme → Huge APP** (or another scheme with at least 2 MB for the app). ESP32-S3 and ESP32-C3 `BLE_Minimal` sketches fit the default partition.

## Channel names and packet size

Maximum channel name length is **32** characters. Longer keys are ignored; they are never truncated. The store holds up to **32** channels.

Incoming packets are limited to **512** bytes on ESP8266 and UNO R4 WiFi, and **1024** bytes on ESP32. A snapshot that does not fit is discarded and all channels go to `0`.

Typical AzDeck keys such as `move_x` fit 32 JSON channels in those packet limits. Thirty-two channels with 32-character names need more than 512 bytes and will not fit on ESP8266 / UNO R4 WiFi.

## Limits for v0.2.1

- JSON on the wire; channel names match the AzDeck profile
- Access Point only; no router / Station Mode
- Web settings page is opt-in (`settings()` + WebSocket HTTP `/settings`)
- No TCP, UDP, labels, or robot/motor APIs
- Telemetry is `send()` only (numbers or short strings)
- Telemetry store: **32** keys on ESP32/ESP8266, **8** keys on UNO R4 WiFi
- ArduinoJson 6.x only (`StaticJsonDocument`)
