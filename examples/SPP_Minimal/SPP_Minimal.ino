#include <AzDeck.h>

AzDeck deck;

void setup() {
    deck.begin(SPP, TEXT);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
}
