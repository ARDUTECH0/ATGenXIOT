# ATGenX Hub Library

# ATGenX

**Professional MQTT Relay & Sensor Control for ESP32 / ESP8266**

ATGenX is an Arduino library that connects your board to an MQTT broker and lets you control relay outputs and read sensors with minimal sketch code. One `hub.loop()` call drives everything — WiFi reconnection, MQTT keepalive, sensor polling, and state publishing.

---

## Features

* Control up to **16 relay outputs** over MQTT
* Read up to **8 sensors** (DHT, PIR, LDR, Ultrasonic — or your own)
* **Automatic MQTT reconnection** — session drops are handled silently
* **Retained state publishing** — broker remembers last known relay state
* **Auto-discovery** — periodic JSON announcement so dashboards detect the board
* Plain-text and JSON command payloads accepted
* Supports **ESP32** (all variants) and **ESP8266**

---

## Installation

### Arduino Library Manager *(recommended)*

1. Open Arduino IDE → **Sketch → Include Library → Manage Libraries**
2. Search for `ATGenX`
3. Click **Install**

### Manual

1. Download the ZIP from the [Releases](https://github.com/yourname/ATGenX/releases) page
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library**

### Dependencies

Install these from Library Manager before using ATGenX:


| Library                                         | Minimum Version |
| ----------------------------------------------- | --------------- |
| PubSubClient                                    | 2.8             |
| ArduinoJson                                     | 7.x             |
| DHT sensor library*(only if using ATGenX\_DHT)* | 1.4             |

---

## Quick Start

```cpp
#include <ATGenX.h>

ATGenX_Hub    hub("user123");
ATGenX_Device relay1(5,  "relay1");   // GPIO 5
ATGenX_Device relay2(18, "relay2");   // GPIO 18

void setup() {
    Serial.begin(115200);
    hub.begin("MY_SSID", "MY_PASS", "mqttUser", "mqttPass");
    hub.attach(relay1);
    hub.attach(relay2);
}

void loop() {
    hub.loop();
}
```

Send `on` or `off` to `atgenx/user123/relay1/cmd` — done.

---

## Topic Convention


| Direction           | Topic                                | Example                       |
| ------------------- | ------------------------------------ | ----------------------------- |
| Command (→ device) | `atgenx/<userId>/<deviceId>/cmd`     | `atgenx/user123/relay1/cmd`   |
| State (← device)   | `atgenx/<userId>/<deviceId>/state`   | `atgenx/user123/relay1/state` |
| Reading (← sensor) | `atgenx/<userId>/<sensorId>/reading` | `atgenx/user123/dht1/reading` |
| Discovery           | `atgenx/<userId>/discovery`          | `atgenx/user123/discovery`    |

---

## Accepted Command Payloads

Both plain-text and JSON are supported:

```
Plain : 1 | 0 | on | off | true | false | toggle
JSON  : {"state":1} | {"state":"ON"} | {"state":"off"} | {"state":"toggle"}
```

---

## Sensor Payloads

Each sensor publishes JSON to its `/reading` topic:


| Sensor             | Payload                                     |
| ------------------ | ------------------------------------------- |
| DHT11 / DHT22      | `{"tempC":24.5,"humidity":61.2,"ts":12345}` |
| PIR                | `{"motion":1,"ts":12345}`                   |
| LDR                | `{"analog":2047,"percent":50,"ts":12345}`   |
| HC-SR04 Ultrasonic | `{"value":23.5,"unit":"cm","ts":12345}`     |

---

## API Reference

### ATGenX\_Hub

```cpp
// Construction
ATGenX_Hub hub("userId");

// Lifecycle
hub.begin(ssid, password, mqttUser, mqttPass);  // call once in setup()
hub.loop();                                      // call every loop()

// Device & sensor registry
hub.attach(device);         // register a relay output
hub.attachSensor(sensor);   // register a sensor

// Query
hub.isConnected();          // bool — MQTT session active?
hub.getUserId();            // const char*
hub.getBoardType();         // "ESP32" | "ESP8266" | ...
hub.deviceCount();          // size_t
hub.sensorCount();          // size_t
hub.getDevice(index);       // const ATGenX_Device*
hub.getSensor(index);       // const ATGenX_Sensor*

// Callbacks
hub.onError([](ATGenX_Error e, const char* detail) { ... });
hub.onConnectionChange([](bool connected) { ... });
```

**Error codes (`ATGenX_Error`):**


| Code             | Meaning                                       |
| ---------------- | --------------------------------------------- |
| `WIFI_TIMEOUT`   | WiFi association timed out — board restarted |
| `MQTT_FAILED`    | All MQTT retry attempts exhausted             |
| `MQTT_RECONNECT` | Session dropped — reconnection started       |
| `DEVICE_LIMIT`   | `attach()`called after`MAX_DEVICES`(16)       |
| `SENSOR_LIMIT`   | `attachSensor()`called after`MAX_SENSORS`(8)  |
| `PUBLISH_FAILED` | MQTT`publish()`returned false                 |

---

### ATGenX\_Device

```cpp
ATGenX_Device relay(pin, "deviceId", activeLow = true);

// Control
relay.turnOn();
relay.turnOff();
relay.toggle();

// Query
relay.isOn();        // bool
relay.getPin();      // uint8_t
relay.getId();       // const char*
relay.getFullPath(); // const char*  — "userId/deviceId"

// Callback
relay.onStateChange([](bool on) { ... });
```

---

### ATGenX\_Sensor (base class)

```cpp
// All built-in sensors inherit from this.

sensor.publishNow();             // force immediate read + publish
sensor.setInterval(ms);          // change polling period at runtime (0 = pause)
sensor.getId();                  // const char*
sensor.getFullPath();            // const char*
sensor.getTopic();               // const char*  — full /reading topic
```

---

### Built-in Sensors

```cpp
// DHT11 / DHT22
ATGenX_DHT dht(pin, "dht1", DHT22, intervalMs = 5000);
dht.begin();

// PIR motion sensor
ATGenX_PIR pir(pin, "pir1", intervalMs = 500);
pir.begin();

// LDR light sensor
ATGenX_LDR ldr(pin, "ldr1", adcMax = 4095, intervalMs = 2000);
ldr.begin();

// HC-SR04 ultrasonic distance sensor
ATGenX_Ultrasonic us(trigPin, echoPin, "us1", intervalMs = 1000);
us.begin();
```

---

### ATGenX\_Discovery

```cpp
ATGenX_Discovery discovery(hub, intervalMs = 30000);

discovery.begin();   // call in setup() AFTER hub.begin() and all attach() calls
discovery.loop();    // call every loop()
discovery.announce(); // force immediate re-announcement
discovery.setInterval(ms);
discovery.getTopic(); // const char*
```

---

## Full Example

```cpp
#include <ATGenX.h>

ATGenX_Hub          hub("user123");

// Outputs
ATGenX_Device       relay1(5,  "relay1");
ATGenX_Device       relay2(18, "relay2");

// Sensors
ATGenX_DHT          dht(4,     "dht1");
ATGenX_PIR          pir(13,    "pir1");
ATGenX_LDR          ldr(34,    "ldr1");
ATGenX_Ultrasonic   us(12, 14, "us1");

// Discovery
ATGenX_Discovery    discovery(hub, 30000);

void setup() {
    Serial.begin(115200);

    hub.onError([](ATGenX_Error e, const char* d) {
        Serial.printf("[Error %u] %s\n", (uint8_t)e, d);
    });

    hub.begin("MY_SSID", "MY_PASS", "mqttUser", "mqttPass");

    hub.attach(relay1);
    hub.attach(relay2);

    hub.attachSensor(dht);
    hub.attachSensor(pir);
    hub.attachSensor(ldr);
    hub.attachSensor(us);

    dht.begin();
    pir.begin();
    ldr.begin();
    us.begin();

    discovery.begin();

    relay1.onStateChange([](bool on) {
        Serial.printf("relay1 is now %s\n", on ? "ON" : "OFF");
    });
}

void loop() {
    hub.loop();
    discovery.loop();
}
```

---

## Adding a Custom Sensor

Subclass `ATGenX_Sensor` and override one method:

```cpp
#include <ATGenX.h>

class ATGenX_MySensor : public ATGenX_Sensor {
public:
    ATGenX_MySensor(uint8_t pin, const char* id)
        : ATGenX_Sensor(id, 2000), _pin(pin) {}

    void begin() { pinMode(_pin, INPUT); }

protected:
    bool readAndBuildPayload(char* buf, size_t sz) override {
        int v = analogRead(_pin);
        snprintf(buf, sz, "{\"value\":%d,\"ts\":%lu}", v, millis());
        return true;  // return false to skip publishing
    }

private:
    uint8_t _pin;
};
```

Then use it exactly like any built-in sensor:

```cpp
ATGenX_MySensor mySensor(35, "my1");
hub.attachSensor(mySensor);
mySensor.begin();
```

The sensor will publish to `atgenx/<userId>/my1/reading` every 2 seconds automatically.

---

## Hardware Notes

### Relay wiring (`activeLow`)

Most relay modules trigger on **LOW** signal — this is the default (`activeLow = true`).
Pass `false` as the third constructor argument for active-HIGH modules:

```cpp
ATGenX_Device relay(5, "relay1", false);   // active HIGH
```

### HC-SR04 on 3.3 V boards

The ECHO pin outputs 5 V. Use a voltage divider (1 kΩ + 2 kΩ) to protect the GPIO.

### DHT22 minimum interval

The DHT22 needs at least **2000 ms** between readings. The default interval is 5000 ms.

### ESP8266 ADC

The ESP8266 has a 10-bit ADC (max = 1023). Pass this as `adcMax`:

```cpp
ATGenX_LDR ldr(A0, "ldr1", 1023);
```

---

## Library Constants


| Constant           | Default | Description                             |
| ------------------ | ------- | --------------------------------------- |
| `MAX_DEVICES`      | 16      | Max relay outputs per hub               |
| `MAX_SENSORS`      | 8       | Max sensors per hub                     |
| `WIFI_TIMEOUT_SEC` | 30      | WiFi association timeout before restart |
| `MQTT_MAX_RETRIES` | 5       | MQTT connection attempts before error   |
| `MQTT_RETRY_MS`    | 3000    | Delay between MQTT retry attempts       |
| `MQTT_KEEPALIVE_S` | 60      | MQTT keepalive interval                 |

---

## License

MIT — see [LICENSE](https://claude.ai/chat/LICENSE) file.

# ATGenX

**Professional MQTT Relay & Sensor Control for ESP32 / ESP8266**

ATGenX is an Arduino library that connects your board to an MQTT broker and lets you control relay outputs and read sensors with minimal sketch code. One `hub.loop()` call drives everything — WiFi reconnection, MQTT keepalive, sensor polling, and state publishing.

---

## Features

* Control up to **16 relay outputs** over MQTT
* Read up to **8 sensors** (DHT, PIR, LDR, Ultrasonic — or your own)
* **Automatic MQTT reconnection** — session drops are handled silently
* **Retained state publishing** — broker remembers last known relay state
* **Auto-discovery** — periodic JSON announcement so dashboards detect the board
* Plain-text and JSON command payloads accepted
* Supports **ESP32** (all variants) and **ESP8266**

---

## Installation

### Arduino Library Manager *(recommended)*

1. Open Arduino IDE → **Sketch → Include Library → Manage Libraries**
2. Search for `ATGenX`
3. Click **Install**

### Manual

1. Download the ZIP from the [Releases](https://github.com/yourname/ATGenX/releases) page
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library**

### Dependencies

Install these from Library Manager before using ATGenX:


| Library                                         | Minimum Version |
| ----------------------------------------------- | --------------- |
| PubSubClient                                    | 2.8             |
| ArduinoJson                                     | 7.x             |
| DHT sensor library*(only if using ATGenX\_DHT)* | 1.4             |

---

## Quick Start

```cpp
#include <ATGenX.h>

ATGenX_Hub    hub("user123");
ATGenX_Device relay1(5,  "relay1");   // GPIO 5
ATGenX_Device relay2(18, "relay2");   // GPIO 18

void setup() {
    Serial.begin(115200);
    hub.begin("MY_SSID", "MY_PASS", "mqttUser", "mqttPass");
    hub.attach(relay1);
    hub.attach(relay2);
}

void loop() {
    hub.loop();
}
```

Send `on` or `off` to `atgenx/user123/relay1/cmd` — done.

---

## Topic Convention


| Direction           | Topic                                | Example                       |
| ------------------- | ------------------------------------ | ----------------------------- |
| Command (→ device) | `atgenx/<userId>/<deviceId>/cmd`     | `atgenx/user123/relay1/cmd`   |
| State (← device)   | `atgenx/<userId>/<deviceId>/state`   | `atgenx/user123/relay1/state` |
| Reading (← sensor) | `atgenx/<userId>/<sensorId>/reading` | `atgenx/user123/dht1/reading` |
| Discovery           | `atgenx/<userId>/discovery`          | `atgenx/user123/discovery`    |

---

## Accepted Command Payloads

Both plain-text and JSON are supported:

```
Plain : 1 | 0 | on | off | true | false | toggle
JSON  : {"state":1} | {"state":"ON"} | {"state":"off"} | {"state":"toggle"}
```

---

## Sensor Payloads

Each sensor publishes JSON to its `/reading` topic:


| Sensor             | Payload                                     |
| ------------------ | ------------------------------------------- |
| DHT11 / DHT22      | `{"tempC":24.5,"humidity":61.2,"ts":12345}` |
| PIR                | `{"motion":1,"ts":12345}`                   |
| LDR                | `{"analog":2047,"percent":50,"ts":12345}`   |
| HC-SR04 Ultrasonic | `{"value":23.5,"unit":"cm","ts":12345}`     |

---

## API Reference

### ATGenX\_Hub

```cpp
// Construction
ATGenX_Hub hub("userId");

// Lifecycle
hub.begin(ssid, password, mqttUser, mqttPass);  // call once in setup()
hub.loop();                                      // call every loop()

// Device & sensor registry
hub.attach(device);         // register a relay output
hub.attachSensor(sensor);   // register a sensor

// Query
hub.isConnected();          // bool — MQTT session active?
hub.getUserId();            // const char*
hub.getBoardType();         // "ESP32" | "ESP8266" | ...
hub.deviceCount();          // size_t
hub.sensorCount();          // size_t
hub.getDevice(index);       // const ATGenX_Device*
hub.getSensor(index);       // const ATGenX_Sensor*

// Callbacks
hub.onError([](ATGenX_Error e, const char* detail) { ... });
hub.onConnectionChange([](bool connected) { ... });
```

**Error codes (`ATGenX_Error`):**


| Code             | Meaning                                       |
| ---------------- | --------------------------------------------- |
| `WIFI_TIMEOUT`   | WiFi association timed out — board restarted |
| `MQTT_FAILED`    | All MQTT retry attempts exhausted             |
| `MQTT_RECONNECT` | Session dropped — reconnection started       |
| `DEVICE_LIMIT`   | `attach()`called after`MAX_DEVICES`(16)       |
| `SENSOR_LIMIT`   | `attachSensor()`called after`MAX_SENSORS`(8)  |
| `PUBLISH_FAILED` | MQTT`publish()`returned false                 |

---

### ATGenX\_Device

```cpp
ATGenX_Device relay(pin, "deviceId", activeLow = true);

// Control
relay.turnOn();
relay.turnOff();
relay.toggle();

// Query
relay.isOn();        // bool
relay.getPin();      // uint8_t
relay.getId();       // const char*
relay.getFullPath(); // const char*  — "userId/deviceId"

// Callback
relay.onStateChange([](bool on) { ... });
```

---

### ATGenX\_Sensor (base class)

```cpp
// All built-in sensors inherit from this.

sensor.publishNow();             // force immediate read + publish
sensor.setInterval(ms);          // change polling period at runtime (0 = pause)
sensor.getId();                  // const char*
sensor.getFullPath();            // const char*
sensor.getTopic();               // const char*  — full /reading topic
```

---

### Built-in Sensors

```cpp
// DHT11 / DHT22
ATGenX_DHT dht(pin, "dht1", DHT22, intervalMs = 5000);
dht.begin();

// PIR motion sensor
ATGenX_PIR pir(pin, "pir1", intervalMs = 500);
pir.begin();

// LDR light sensor
ATGenX_LDR ldr(pin, "ldr1", adcMax = 4095, intervalMs = 2000);
ldr.begin();

// HC-SR04 ultrasonic distance sensor
ATGenX_Ultrasonic us(trigPin, echoPin, "us1", intervalMs = 1000);
us.begin();
```

---

### ATGenX\_Discovery

```cpp
ATGenX_Discovery discovery(hub, intervalMs = 30000);

discovery.begin();   // call in setup() AFTER hub.begin() and all attach() calls
discovery.loop();    // call every loop()
discovery.announce(); // force immediate re-announcement
discovery.setInterval(ms);
discovery.getTopic(); // const char*
```

---

## Full Example

```cpp
#include <ATGenX.h>

ATGenX_Hub          hub("user123");

// Outputs
ATGenX_Device       relay1(5,  "relay1");
ATGenX_Device       relay2(18, "relay2");

// Sensors
ATGenX_DHT          dht(4,     "dht1");
ATGenX_PIR          pir(13,    "pir1");
ATGenX_LDR          ldr(34,    "ldr1");
ATGenX_Ultrasonic   us(12, 14, "us1");

// Discovery
ATGenX_Discovery    discovery(hub, 30000);

void setup() {
    Serial.begin(115200);

    hub.onError([](ATGenX_Error e, const char* d) {
        Serial.printf("[Error %u] %s\n", (uint8_t)e, d);
    });

    hub.begin("MY_SSID", "MY_PASS", "mqttUser", "mqttPass");

    hub.attach(relay1);
    hub.attach(relay2);

    hub.attachSensor(dht);
    hub.attachSensor(pir);
    hub.attachSensor(ldr);
    hub.attachSensor(us);

    dht.begin();
    pir.begin();
    ldr.begin();
    us.begin();

    discovery.begin();

    relay1.onStateChange([](bool on) {
        Serial.printf("relay1 is now %s\n", on ? "ON" : "OFF");
    });
}

void loop() {
    hub.loop();
    discovery.loop();
}
```

---

## Adding a Custom Sensor

Subclass `ATGenX_Sensor` and override one method:

```cpp
#include <ATGenX.h>

class ATGenX_MySensor : public ATGenX_Sensor {
public:
    ATGenX_MySensor(uint8_t pin, const char* id)
        : ATGenX_Sensor(id, 2000), _pin(pin) {}

    void begin() { pinMode(_pin, INPUT); }

protected:
    bool readAndBuildPayload(char* buf, size_t sz) override {
        int v = analogRead(_pin);
        snprintf(buf, sz, "{\"value\":%d,\"ts\":%lu}", v, millis());
        return true;  // return false to skip publishing
    }

private:
    uint8_t _pin;
};
```

Then use it exactly like any built-in sensor:

```cpp
ATGenX_MySensor mySensor(35, "my1");
hub.attachSensor(mySensor);
mySensor.begin();
```

The sensor will publish to `atgenx/<userId>/my1/reading` every 2 seconds automatically.

---

## Hardware Notes

### Relay wiring (`activeLow`)

Most relay modules trigger on **LOW** signal — this is the default (`activeLow = true`).
Pass `false` as the third constructor argument for active-HIGH modules:

```cpp
ATGenX_Device relay(5, "relay1", false);   // active HIGH
```

### HC-SR04 on 3.3 V boards

The ECHO pin outputs 5 V. Use a voltage divider (1 kΩ + 2 kΩ) to protect the GPIO.

### DHT22 minimum interval

The DHT22 needs at least **2000 ms** between readings. The default interval is 5000 ms.

### ESP8266 ADC

The ESP8266 has a 10-bit ADC (max = 1023). Pass this as `adcMax`:

```cpp
ATGenX_LDR ldr(A0, "ldr1", 1023);
```

---

## Library Constants


| Constant           | Default | Description                             |
| ------------------ | ------- | --------------------------------------- |
| `MAX_DEVICES`      | 16      | Max relay outputs per hub               |
| `MAX_SENSORS`      | 8       | Max sensors per hub                     |
| `WIFI_TIMEOUT_SEC` | 30      | WiFi association timeout before restart |
| `MQTT_MAX_RETRIES` | 5       | MQTT connection attempts before error   |
| `MQTT_RETRY_MS`    | 3000    | Delay between MQTT retry attempts       |
| `MQTT_KEEPALIVE_S` | 60      | MQTT keepalive interval                 |

---

## License

MIT — see [LICENSE](https://claude.ai/chat/LICENSE) file.

Professional MQTT Relay Control for ATGenX Platform

## Features

- ✅ Single MQTT connection for multiple devices
- ✅ Easy configuration
- ✅ Auto-reconnect
- ✅ JSON and raw string commands
- ✅ Callback support

## Installation

1. Download as ZIP
2. Arduino IDE → Sketch → Include Library → Add .ZIP Library

## Quick Start

### Single Device

```cpp
#include <ATGenX_Hub.h>
#include <ATGenX_Device.h>

ATGenX_Hub hub("atg_dev_6515fa");
ATGenX_Device relay(8, "1");

void setup() {
  hub.begin("SSID", "PASS", "MQTT_USER", "MQTT_PASS");
  hub.attachDevice(&relay);
}

void loop() {
  hub.loop();
}
```
