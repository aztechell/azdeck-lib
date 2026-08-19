#include <AzDeck.h>

AzDeck deck;

void setup() {
    deck.begin(BLE, JSON);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
}
