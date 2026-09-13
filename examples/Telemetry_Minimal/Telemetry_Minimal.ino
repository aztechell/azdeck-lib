// AzDeck app: https://play.google.com/store/apps/details?id=com.aztechell.azdeck
// Wi-Fi AzDeck / azdeck123, ws://192.168.4.1:81, JSON. Label channel: count
#include <AzDeck.h>

AzDeck deck;
int n = 0;
uint32_t lastMs = 0;

void setup() {
    deck.begin(WEBSOCKET, JSON);
}

void loop() {
    deck.update();

    uint32_t now = millis();
    if (now - lastMs < 200) {
        return;
    }
    lastMs = now;

    deck.send("count", n);
    n += 1;
    if (n > 1000) {
        n = 0;
    }
}
