// AzDeck app: https://play.google.com/store/apps/details?id=com.aztechell.azdeck
#include <AzDeck.h>

AzDeck deck;

void setup() {
  Serial.begin(9600);
  deck.begin(WEBSOCKET, JSON);
}

void loop() {
  deck.update();

  float x = deck.axis("move_x");
  float y = deck.axis("move_y");

  Serial.print("X = ");
  Serial.print(x);
  Serial.print(" Y = ");
  Serial.println(y);
}
