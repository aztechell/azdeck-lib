// AzDeck app: https://play.google.com/store/apps/details?id=com.aztechell.azdeck
#include <AzDeck.h>

AzDeck deck;

void setup() {
    deck.begin(BLE);
}

void loop() {
    deck.update();

    float x = deck.axis("move_x");
    float y = deck.axis("move_y");

    float speed = deck.slider("speed");

    bool l2 = deck.button("l2");

    bool up = deck.dpad("up");

    float custom = deck.value("custom_channel");
}
