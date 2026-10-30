/**
 * Phone_WiFi_Setup — no Wi-Fi name or password in this sketch.
 *
 * 1. Upload it once.
 * 2. The board opens the network "ATGenX-XXXX" (its light blinks).
 * 3. In the ATGenX app: My devices → + (Add a device) → follow the steps.
 *    (No app? Join "ATGenX-XXXX" with the password "atgenx-setup" — a page opens by itself.)
 * The board remembers the network. To change it later, hold BOOT for 5 seconds.
 */
#include <ATGenXIOT.h>

ATGenX_Hub    hub("atg_dev_xxxxxx");      // your Client ID (ATGenX → IoT account)
ATGenX_Device lamp(2, "lamp");

void setup() {
    Serial.begin(115200);
    hub.begin("atg_usr_xxxxxx", "atg_sk_xxxxxx");   // the board's key — that's all
    hub.attach(lamp);
}

void loop() {
    hub.loop();
}