/**
 * @file    ATGenX_Hub.cpp
 * @brief   ATGenX_Hub implementation
 * @version 2.1.0
 */

#include "ATGenX_Hub.h"
#include "ATGenX_Device.h"
#include "ATGenX_Sensor.h"

// ═══════════════════════════════════════════════════════════════════════════
// Static members
// ═══════════════════════════════════════════════════════════════════════════

const char*    ATGenX_Hub::_brokerHost = "76.13.52.9";
const uint16_t ATGenX_Hub::_brokerPort = 6523;
ATGenX_Hub*    ATGenX_Hub::_instance   = nullptr;

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

ATGenX_Hub::ATGenX_Hub(const char* userId)
    : _userId(userId),
      _mqtt(_wifiClient),
      _deviceCount(0),
      _sensorCount(0),
      _onError(nullptr),
      _onConnectionChange(nullptr)
{
    _instance = this;

    for (size_t i = 0; i < MAX_DEVICES; ++i) _devices[i] = nullptr;
    for (size_t i = 0; i < MAX_SENSORS; ++i) _sensors[i] = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Hub::begin(const char* ssid,
                       const char* password,
                       const char* mqttUsername,
                       const char* mqttPassword)
{
    _mqttUser = mqttUsername;
    _mqttPass = mqttPassword;
    _clientId = buildClientId();

    // ── Banner ──────────────────────────────────────────────────────────────
    Serial.println();
    Serial.println(F("╔══════════════════════════════════════════╗"));
    Serial.println(F("║        ATGenX Hub  v2.1.0                ║"));
    Serial.println(F("╠══════════════════════════════════════════╣"));
    Serial.print(F("║  Board   : ")); Serial.print(getBoardType());
    Serial.println(F("                         ║"));
    Serial.print(F("║  User    : ")); Serial.print(_userId);
    Serial.println(F("                         ║"));
    Serial.print(F("║  Client  : ")); Serial.print(_clientId);
    Serial.println(F("         ║"));
    Serial.println(F("╚══════════════════════════════════════════╝"));

    // ── Wi-Fi ───────────────────────────────────────────────────────────────
    connectWiFi(ssid, password);

    // ── MQTT ────────────────────────────────────────────────────────────────
    _mqtt.setServer(_brokerHost, _brokerPort);
    _mqtt.setCallback(mqttCallback);
    _mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
    _mqtt.setSocketTimeout(10);

    connectMQTT();
}

void ATGenX_Hub::loop() {
    // ── MQTT health ──────────────────────────────────────────────────────────
    if (!_mqtt.connected()) {
        reportError(ATGenX_Error::MQTT_RECONNECT, "session dropped – reconnecting");
        connectMQTT();
    }
    _mqtt.loop();

    // ── Sensor polling ───────────────────────────────────────────────────────
    for (size_t i = 0; i < _sensorCount; ++i) {
        _sensors[i]->loop();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Device registry
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Hub::attach(ATGenX_Device& device) {
    if (_deviceCount >= MAX_DEVICES) {
        reportError(ATGenX_Error::DEVICE_LIMIT, device.getId());
        return;
    }

    _devices[_deviceCount++] = &device;
    device.attachTo(this);

    // Subscribe now if already connected; connectMQTT() handles it otherwise
    if (_mqtt.connected()) {
        const String topic = "atgenx/" + _userId + "/" + device.getId() + "/cmd";
        subscribe(topic.c_str());
        device.publishState();
    }

    Serial.print(F("[ATGenX] Attached device '"));
    Serial.print(device.getId());
    Serial.print(F("'  GPIO="));
    Serial.println(device.getPin());
}

size_t ATGenX_Hub::deviceCount() const { return _deviceCount; }

const ATGenX_Device* ATGenX_Hub::getDevice(size_t index) const {
    if (index >= _deviceCount) return nullptr;
    return _devices[index];
}

// ═══════════════════════════════════════════════════════════════════════════
// Sensor registry
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Hub::attachSensor(ATGenX_Sensor& sensor) {
    if (_sensorCount >= MAX_SENSORS) {
        reportError(ATGenX_Error::SENSOR_LIMIT, sensor.getId());
        return;
    }

    _sensors[_sensorCount++] = &sensor;
    sensor.attachTo(this);

    Serial.print(F("[ATGenX] Attached sensor '"));
    Serial.print(sensor.getId());
    Serial.println('\'');
}

size_t ATGenX_Hub::sensorCount() const { return _sensorCount; }

const ATGenX_Sensor* ATGenX_Hub::getSensor(size_t index) const {
    if (index >= _sensorCount) return nullptr;
    return _sensors[index];
}

// ═══════════════════════════════════════════════════════════════════════════
// Status
// ═══════════════════════════════════════════════════════════════════════════

bool ATGenX_Hub::isConnected() { return _mqtt.connected(); }

const char* ATGenX_Hub::getUserId() const { return _userId.c_str(); }

const char* ATGenX_Hub::getBoardType() const {
#if defined(ESP32)
#   if   CONFIG_IDF_TARGET_ESP32C3
        return "ESP32-C3";
#   elif CONFIG_IDF_TARGET_ESP32S2
        return "ESP32-S2";
#   elif CONFIG_IDF_TARGET_ESP32S3
        return "ESP32-S3";
#   else
        return "ESP32";
#   endif
#elif defined(ESP8266)
    return "ESP8266";
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Hub::onError(void (*cb)(ATGenX_Error, const char*)) {
    _onError = cb;
}

void ATGenX_Hub::onConnectionChange(void (*cb)(bool)) {
    _onConnectionChange = cb;
}

// ═══════════════════════════════════════════════════════════════════════════
// Internal publish / subscribe
// ═══════════════════════════════════════════════════════════════════════════

bool ATGenX_Hub::publish(const char* topic, const char* payload, bool retained) {
    const bool ok = _mqtt.publish(topic, payload, retained);
    if (!ok) reportError(ATGenX_Error::PUBLISH_FAILED, topic);
    return ok;
}

bool ATGenX_Hub::subscribe(const char* topic) {
    return _mqtt.subscribe(topic, /*qos=*/1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Private helpers
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Hub::connectWiFi(const char* ssid, const char* pass) {
    Serial.print(F("[WiFi] Connecting to \""));
    Serial.print(ssid);
    Serial.println('"');

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    const unsigned long deadline = millis() + (WIFI_TIMEOUT_SEC * 1000UL);
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() > deadline) {
            reportError(ATGenX_Error::WIFI_TIMEOUT, ssid);
            Serial.println(F("[WiFi] Timeout – restarting"));
            ESP.restart();
        }
        delay(500);
        Serial.print('.');
    }

    Serial.println();
    Serial.print(F("[WiFi] Connected  IP="));
    Serial.println(WiFi.localIP());
}

void ATGenX_Hub::connectMQTT() {
    for (uint8_t attempt = 1; attempt <= MQTT_MAX_RETRIES; ++attempt) {
        Serial.print(F("[MQTT] Attempt "));
        Serial.print(attempt);
        Serial.print('/');
        Serial.print(MQTT_MAX_RETRIES);
        Serial.print(F(" ... "));

        if (_mqtt.connect(_clientId.c_str(),
                          _mqttUser.c_str(),
                          _mqttPass.c_str()))
        {
            Serial.println(F("connected"));
            onMqttConnected();
            return;
        }

        Serial.print(F("failed (rc="));
        Serial.print(_mqtt.state());
        Serial.println(')');

        if (attempt < MQTT_MAX_RETRIES) delay(MQTT_RETRY_MS);
    }

    reportError(ATGenX_Error::MQTT_FAILED, _brokerHost);
}

void ATGenX_Hub::onMqttConnected() {
    // Resubscribe to all registered output devices
    for (size_t i = 0; i < _deviceCount; ++i) {
        const String topic = "atgenx/" + _userId + "/" + _devices[i]->getId() + "/cmd";
        subscribe(topic.c_str());
        _devices[i]->publishState();
    }

    // Sensors don't subscribe; they push — nothing to do here for them.

    if (_onConnectionChange) _onConnectionChange(true);
}

void ATGenX_Hub::handleMessage(char* topic, byte* payload, unsigned int len) {
    // Topic structure: atgenx/<userId>/<deviceId>/cmd
    const String t(topic);

    const int p1 = t.indexOf('/');
    if (p1 < 0) return;
    const int p2 = t.indexOf('/', p1 + 1);
    if (p2 < 0) return;
    const int p3 = t.indexOf('/', p2 + 1);
    if (p3 < 0) return;

    const String deviceId = t.substring(p2 + 1, p3);

    for (size_t i = 0; i < _deviceCount; ++i) {
        if (deviceId.equals(_devices[i]->getId())) {
            _devices[i]->handleCommand(reinterpret_cast<const char*>(payload), len);
            return;
        }
    }

    Serial.print(F("[ATGenX] Unrouted message for device '"));
    Serial.print(deviceId);
    Serial.println('\'');
}

void ATGenX_Hub::reportError(ATGenX_Error error, const char* detail) const {
    Serial.print(F("[ATGenX] Error "));
    Serial.print(static_cast<uint8_t>(error));
    Serial.print(F(" – "));
    Serial.println(detail);

    if (_onError) _onError(error, detail);
}

String ATGenX_Hub::buildClientId() const {
    String id = F("atgx-");

#if defined(ESP32)
    const uint64_t mac = ESP.getEfuseMac();
    id += String(static_cast<uint32_t>(mac >> 32), HEX);
    id += String(static_cast<uint32_t>(mac),        HEX);
#elif defined(ESP8266)
    id += String(ESP.getChipId(), HEX);
#else
    id += String(millis());
#endif

    return id;
}

void ATGenX_Hub::mqttCallback(char* topic, byte* payload, unsigned int len) {
    if (_instance) _instance->handleMessage(topic, payload, len);
}