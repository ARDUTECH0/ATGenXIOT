# ATGenX Hub (ATGenXIOT)

Connect an **ESP32 or ESP8266** to the [ATGENX](https://atgenx.com) IoT cloud.
Switch outputs from the dashboard, stream sensor readings into history and
charts, and let cloud automations react — with one MQTT connection.

- **Outputs:** relays, LEDs, buzzers, motors (`ATGenX_Device`), servos (`ATGenX_Servo`)
- **Sensors:** DHT11/22, PIR, LDR, any analog sensor, any ON/OFF input, HC-SR04, sound
- **Robust:** non-blocking reconnect with back-off, so outputs and sensors keep running while Wi-Fi or the broker is down
- **Presence:** online/offline status topic (MQTT last will) and periodic board discovery
- **Friendly errors:** "wrong MQTT username/password", "device limit reached", … on Serial and in `onError()`

## Install

**Library Manager:** search for **ATGenX_Hub** and install it, along with its
dependencies (PubSubClient, ArduinoJson, DHT sensor library).
For servos on ESP32, also install **ESP32Servo**.

**Manual:** download this repository as a ZIP, then choose *Sketch → Include Library → Add .ZIP Library*.

## Your credentials

On atgenx.com go to **Billing → IoT account**. There you'll find:

| Field          | Example            | Used as                      |
|----------------|--------------------|------------------------------|
| MQTT username  | `atg_usr_…`        | 3rd argument of `hub.begin`  |
| Device password| `atg_sk_…`         | 4th argument of `hub.begin`  |
| Client ID      | `atg_dev_…`        | `ATGenX_Hub hub("atg_dev_…")`|

The ATGENX Builder can fill these in for you: select the board, then **Use my ATGENX IoT account**.

## Quick start

```cpp
#include <ATGenXIOT.h>

ATGenX_Hub    hub("atg_dev_xxxxxx");          // Client ID
ATGenX_Device lamp(2, "lamp", false);          // LED on GPIO2 (relay modules: true)
ATGenX_DHT    room(4, "room", DHT22);

void setup() {
    Serial.begin(115200);
    hub.begin("MyWiFi", "wifi-password", "atg_usr_xxxxxxxx", "atg_sk_xxxxxxxx");
    hub.attach(lamp);
    hub.attachSensor(room);
    room.begin();
}

void loop() {
    hub.loop();        // keep loop() free of long delay() calls
}
```

More examples are in `examples/`: Single_Relay, Multi_Relay, Smart_Room and Servo_Door.

## Classes

| Class | Constructor | Publishes |
|---|---|---|
| `ATGenX_Device` | `(pin, "id", activeLow = true)` | state `{"state":1}` |
| `ATGenX_Servo`  | `(pin, "id", startAngle = 0)`, needs `#include <ATGenX_Servo.h>` | state `{"angle":90,"state":1}` |
| `ATGenX_DHT` | `(pin, "id", DHT22, intervalMs = 5000)` | `{"tempC":24.5,"humidity":58.0}` |
| `ATGenX_PIR` | `(pin, "id")` | `{"motion":1}` on change |
| `ATGenX_Digital` | `(pin, "id", activeLow = true, debounceMs = 30)` | `{"state":1}` on change |
| `ATGenX_Analog` | `(pin, "id", intervalMs = 1000, minChange = 20, heartbeatMs = 30000)` | `{"value":2048,"percent":50}` |
| `ATGenX_LDR` | `(pin, "id", adcMax = 4095)` | `{"value":…,"percent":…,"analog":…}` |
| `ATGenX_Ultrasonic` | `(trigPin, echoPin, "id")` | `{"value":42.0,"unit":"cm"}` |
| `ATGenX_SoundSensor` | `(digitalPin, analogPin, "id", mode, intervalMs, threshold)` | `{"state":…,"value":…}` |
| `ATGenX_Discovery` | `(hub, intervalMs = 30000)` | board announcement |

`ATGenX.h` is kept as an alias of `ATGenXIOT.h` for older sketches.

### Hub

```cpp
hub.setServer("192.168.1.10", 1883);   // optional, before begin() (local broker)
hub.begin(ssid, pass, mqttUser, mqttPass);
hub.loop();
hub.onError([](ATGenX_Error e, const char* why) { … });
hub.onConnectionChange([](bool online) { … });
hub.isConnected();  hub.getClientId();  hub.getBoardType();
```

### Outputs

```cpp
lamp.turnOn();  lamp.turnOff();  lamp.toggle();  lamp.isOn();
lamp.onStateChange([](bool on) { … });
door.write(90);  door.angle();           // ATGenX_Servo
```

## Topics

`<clientId>` is your Client ID; `<id>` is the name you gave the device or sensor.

| Topic | Direction | Payload |
|---|---|---|
| `atgenx/<clientId>/<id>/cmd` | cloud → board | `{"state":1}` · `{"state":"off"}` · `{"state":true}` · `{"cmd":"toggle"}` · `{"cmd":"getState"}` · `{"angle":90}` · plain `on` / `off` / `1` / `0` |
| `atgenx/<clientId>/<id>/state` | board → cloud (retained) | `{"state":1,"label":"ON",…}` |
| `atgenx/<clientId>/<id>/reading` | board → cloud | sensor JSON (see above) |
| `atgenx/<clientId>/<boardId>/status` | board → cloud (retained, last will) | `{"online":true,"board":"ESP32","ip":"…","rssi":-60}` / `{"online":false}` |
| `atgenx/<clientId>/discovery` | board → cloud (retained) | board, IP, RSSI, uptime, devices |

Commands can be up to 255 bytes. The cloud's automation payloads (`{"state":1,"stateLabel":"ON","source":"automation","ts":…}`) fit comfortably.

## Errors (`onError`)

| Code | Meaning |
|---|---|
| `WIFI_TIMEOUT` | No Wi-Fi within 30 s at start-up. The board restarts. |
| `MQTT_FAILED` | Wrong username/password, an expired plan, or the device limit is reached. |
| `MQTT_RECONNECT` | The connection dropped. Reconnecting in the background (2 s → 60 s back-off). |
| `DEVICE_LIMIT` / `SENSOR_LIMIT` | More than 16 outputs or 24 sensors were attached. |
| `PUBLISH_FAILED` | The broker refused a message (it may be too large). |

## Hardware notes

- **Relays:** most modules switch on LOW, so `activeLow = true` (the default). LEDs, buzzers and MOSFET drivers need `false`.
- **ESP32 analog + Wi-Fi:** use the ADC1 pins (32–39). ADC2 doesn't work while Wi-Fi is on.
- **HC-SR04 on 3.3 V boards:** put a voltage divider on ECHO.
- **DHT22:** read at most every 2 s (the default is 5 s).
- Set `#define ATGENX_DEBUG 1` before the include to log every reading on Serial.

## Over-the-air updates

Boards running ATGenX_Hub 2.3+ update from the ATGENX site — no USB cable:
open the code, choose **Update over the internet**, pick the board. The board
downloads the new firmware, checks its MD5, flashes it and restarts, reporting
`{"ota":"downloading"|"ok"|"failed"}` on its status topic. Cloud builds stamp a
version (`ATGENX_FW_VERSION`) that the board reports as `"fw"` when it comes
online. Turn it off with `hub.enableOta(false)`.

ESP32 sketches need an OTA-capable partition scheme (the default one is).

## Changelog

**2.3.0**
- Over-the-air updates from the ATGENX site (HTTP/HTTPS, MD5-checked, outputs switched off while flashing).
- The online status now includes the firmware version (`"fw"`).

**2.2.0**
- Non-blocking reconnect with back-off; outputs and sensors keep working offline.
- Online/offline status topic (last will). Readings are re-sent right after reconnecting.
- Commands up to 255 bytes. Cloud automations were silently dropped before (64-byte limit).
- Accepts `{"state":true}`, `{"cmd":"getState"}` and `{"cmd":"toggle"}`.
- 1 KB MQTT buffer: discovery announcements are no longer dropped.
- New classes: `ATGenX_Digital`, `ATGenX_Analog`, `ATGenX_Servo`. Added `hub.setServer()`.
- LDR now publishes `value` (the field dashboards read). Quieter Serial output.
- ESP8266 is listed as supported. The examples were filled in (they were empty files).

## License

MIT
