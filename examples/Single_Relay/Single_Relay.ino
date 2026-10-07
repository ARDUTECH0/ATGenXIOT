/*
 * ATGenX — Single relay
 * Switch one relay from the ATGENX dashboard, an automation or a schedule.
 *
 * Get USER / PASS / CLIENT_ID from atgenx.com → Billing → "IoT account".
 */
#include <ATGenXIOT.h>

ATGenX_Hub    hub("atg_dev_xxxxxx");                  // Client ID
ATGenX_Device relay(5, "relay1");                     // most relay modules switch on LOW (default)

void setup() {
    Serial.begin(115200);
    relay.onStateChange([](bool on) { Serial.println(on ? "Relay ON" : "Relay OFF"); });
    hub.begin("YOUR_WIFI", "YOUR_PASSWORD", "atg_usr_xxxxxxxx", "atg_sk_xxxxxxxx");
    hub.attach(relay);
}

void loop() {
    hub.loop();
}
