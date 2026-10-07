/*
 * ATGenX — Smart Room
 * A lamp you switch from the ATGENX dashboard, a DHT22 for temperature and
 * humidity, and a soil/light/gas sensor on an analog pin.
 *
 * Get USER / PASS / CLIENT_ID from atgenx.com → Billing → "IoT account".
 */
#include <ATGenXIOT.h>

static constexpr char WIFI_SSID[] = "YOUR_WIFI";
static constexpr char WIFI_PASS[] = "YOUR_PASSWORD";
static constexpr char USER[]      = "atg_usr_xxxxxxxx";
static constexpr char PASS[]      = "atg_sk_xxxxxxxx";
static constexpr char CLIENT_ID[] = "atg_dev_xxxxxx";

ATGenX_Hub       hub(CLIENT_ID);
ATGenX_Discovery discovery(hub, 30000);

ATGenX_Device lamp(2, "lamp", /*activeLow=*/false);   // LED on GPIO2 (relay modules: true)
ATGenX_DHT    room(4, "room", DHT22);
ATGenX_Analog soil(34, "soil");

void onHubError(ATGenX_Error error, const char* detail) {
    Serial.printf("[App] error %d: %s\n", (int)error, detail);
}

void setup() {
    Serial.begin(115200);
    hub.onError(onHubError);
    hub.onConnectionChange([](bool up) { Serial.println(up ? "[App] online" : "[App] offline"); });

    hub.begin(WIFI_SSID, WIFI_PASS, USER, PASS);
    hub.attach(lamp);
    hub.attachSensor(room);
    hub.attachSensor(soil);
    room.begin();
    soil.begin();
    discovery.begin();
}

void loop() {
    hub.loop();
    discovery.loop();
}
