// AzDeck app: https://play.google.com/store/apps/details?id=com.aztechell.azdeck
// Matrix Mini R4: drive M1/M2, servos RC3/RC4. Install MatrixMiniR4.
// Import profile/Matrix_R4.profile.json in the AzDeck app, then pair BLE.
#include <AzDeck.h>
#include <MatrixMiniR4.h>

AzDeck deck;

void setup() {
    MiniR4.begin();
    MiniR4.PWR.setBattCell(2);
    MiniR4.M1.setBrake(true);
    MiniR4.M2.setBrake(true);
    deck.begin(BLE, JSON);
}

void loop() {
    deck.update();

    int speed = (int)deck.slider("speed");
    int fwd = deck.dpad("up") - deck.dpad("down");
    int turn = deck.dpad("right") - deck.dpad("left");
    MiniR4.M1.setPower((fwd + turn) * speed);
    MiniR4.M2.setPower((fwd - turn) * speed);

    int s1 = (int)deck.slider("ser1");
    int s2 = (int)deck.slider("ser2");
    static int a1 = -1;
    static int a2 = -1;
    if (speed == 0 && fwd == 0 && turn == 0 && s1 == 0 && s2 == 0) {
        return;
    }
    if (s1 != a1) {
        a1 = s1;
        MiniR4.RC3.setAngle(s1);
    }
    if (s2 != a2) {
        a2 = s2;
        MiniR4.RC4.setAngle(s2);
    }
}
