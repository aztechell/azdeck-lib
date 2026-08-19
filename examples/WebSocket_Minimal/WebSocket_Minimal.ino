#include <AzDeck.h>

AzDeck deck;

void setup() {
    deck.begin(WEBSOCKET, JSON);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
    float y = deck.axis("move_y");
}
