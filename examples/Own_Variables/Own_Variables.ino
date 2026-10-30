/*
 * Your own variables — not tied to a part: make them on the site (device page → "Your own variables"),
 * copy each one's ID, and use it in your code.
 *
 *   party   a switch on the dashboard   → your code decides what "party mode" does
 *   speed   a number: a slider on the dashboard sets it, or your code reports it
 *   strip   an RGB colour picker        → drive an RGB LED (or a strip) with it
 *
 * The Wi-Fi is set from the ATGenX phone app (Add a device). Needs ATGenX_Hub 2.6+.
 */
#include <ATGenXIOT.h>

ATGenX_Hub    hub("atg_dev_xxxxxx");      // your Client ID (ATGenX → IoT account)
ATGenX_Switch party("home_party");       // ← the IDs the site gave your variables
ATGenX_Number speed("home_fan_speed");
ATGenX_Color  strip("home_strip");

// an RGB LED on three PWM pins
const uint8_t R = 25, G = 26, B = 27;

void showColor(uint8_t r, uint8_t g, uint8_t b) {
    analogWrite(R, r);
    analogWrite(G, g);
    analogWrite(B, b);
}

void setup() {
    Serial.begin(115200);
    pinMode(R, OUTPUT);
    pinMode(G, OUTPUT);
    pinMode(B, OUTPUT);

    hub.attach(party);
    hub.attach(speed);
    hub.attach(strip);

    party.onChange([](bool on) { Serial.println(on ? "Party mode on" : "Party mode off"); });
    speed.onChange([](float v) { Serial.printf("Speed set to %.0f%%\n", v); });
    strip.onChange(showColor);

    hub.begin("atg_usr_xxxxxx", "atg_sk_xxxxxx");
}

void loop() {
    hub.loop();

    // party mode: the colour runs around the wheel; your code changes the variable, the dashboard follows
    static uint32_t last = 0;
    if (party.isOn() && millis() - last > 2000) {
        last = millis();
        static uint8_t step = 0;
        const uint32_t wheel[] = {0xFF0000, 0xFF8800, 0xFFFF00, 0x00FF00, 0x0088FF, 0x8800FF};
        strip.set(wheel[step++ % 6]);
        showColor(strip.red(), strip.green(), strip.blue());
    }
}
