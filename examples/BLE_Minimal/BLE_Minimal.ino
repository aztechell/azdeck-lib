// AzDeck app: https://play.google.com/store/apps/details?id=com.aztechell.azdeck
#include <AzDeck.h>

AzDeck deck;

void setup() {
    deck.begin(BLE, JSON);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
}
