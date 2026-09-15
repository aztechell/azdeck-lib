// AzDeck app: https://play.google.com/store/apps/details?id=com.aztechell.azdeck
// Wi-Fi AzDeck / azdeck123, ws://192.168.4.1:81
// Settings: http://192.168.4.1/settings
#include <AzDeck.h>

AzDeck deck;

#if defined(ESP8266)
const int kLedOn = LOW;
const int kLedOff = HIGH;
#else
const int kLedOn = HIGH;
const int kLedOff = LOW;
#endif

const char kSettings[] =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>AzDeck</title></head><body>"
    "<h1>AzDeck</h1>"
    "<p><button onclick=\"fetch('/settings?led=1')\">LED on</button> "
    "<button onclick=\"fetch('/settings?led=0')\">LED off</button></p>"
    "</body></html>";

void onSettingsQuery(const char* query) {
    if (query == nullptr) {
        return;
    }
    if (strstr(query, "led=1") != nullptr) {
        digitalWrite(LED_BUILTIN, kLedOn);
    } else if (strstr(query, "led=0") != nullptr) {
        digitalWrite(LED_BUILTIN, kLedOff);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, kLedOff);
    deck.settings(kSettings, onSettingsQuery);
    deck.begin(WEBSOCKET);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
}
