/*
 * ATGenX — 4-channel relay board
 * Each channel is its own device on the dashboard (relay1 … relay4).
 *
 * Get USER / PASS / CLIENT_ID from atgenx.com → Billing → "IoT account".
 */
#include <ATGenXIOT.h>

ATGenX_Hub       hub("atg_dev_xxxxxx");
ATGenX_Discovery discovery(hub, 30000);

ATGenX_Device relays[] = {
    ATGenX_Device(5,  "relay1"),
    ATGenX_Device(18, "relay2"),
    ATGenX_Device(19, "relay3"),
    ATGenX_Device(21, "relay4"),
};

void setup() {
    Serial.begin(115200);
    hub.begin("YOUR_WIFI", "YOUR_PASSWORD", "atg_usr_xxxxxxxx", "atg_sk_xxxxxxxx");
    for (auto& r : relays) hub.attach(r);
    discovery.begin();
}

void loop() {
    hub.loop();
    discovery.loop();
}
