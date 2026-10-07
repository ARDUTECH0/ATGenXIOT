/*
 * ATGenX — Servo door
 * Move a servo from the dashboard slider ({"angle":90}) or an ON/OFF switch.
 * Needs the "ESP32Servo" library on ESP32 (Library Manager).
 */
#include <ATGenXIOT.h>
#include <ATGenX_Servo.h>

ATGenX_Hub   hub("atg_dev_xxxxxx");
ATGenX_Servo door(13, "door");
ATGenX_PIR   hall(27, "hall");

void setup() {
    Serial.begin(115200);
    hub.begin("YOUR_WIFI", "YOUR_PASSWORD", "atg_usr_xxxxxxxx", "atg_sk_xxxxxxxx");
    hub.attach(door);
    door.begin();
    hub.attachSensor(hall);
    hall.begin();
}

void loop() {
    hub.loop();
}
