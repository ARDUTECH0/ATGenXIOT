/**
 * @file    ATGenX_Hub.cpp
 * @brief   ATGenX_Hub implementation
 * @version 2.3.0
 */

#include "ATGenX_Hub.h"
#include "ATGenX_Device.h"
#include "ATGenX_Sensor.h"
#include <ArduinoJson.h>

#if defined(ESP32)
#   include <HTTPUpdate.h>
#   include <WiFiClientSecure.h>
#elif defined(ESP8266)
#   include <ESP8266httpUpdate.h>
#   include <WiFiClientSecureBearSSL.h>
#endif

uint32_t g_atgxFirmwareVersion = 0;
const char* g_atgxProjectId = "";

ATGenX_Hub* ATGenX_Hub::_instance = nullptr;

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

ATGenX_Hub::ATGenX_Hub(const char* userId)
    : _userId(userId),
      _mqtt(_wifiClient),
      _host(ATGENX_BROKER_HOST),
      _port(ATGENX_BROKER_PORT),
      _wasConnected(false),
      _lastAttemptMs(0),
      _retryMs(RETRY_MIN_MS),
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

void ATGenX_Hub::setServer(const char* host, uint16_t port) {
    _host = host;
    _port = port;
}

void ATGenX_Hub::begin(const char* ssid,
                       const char* password,
                       const char* mqttUsername,
                       const char* mqttPassword)
{
    _mqttUser    = mqttUsername;
    _mqttPass    = mqttPassword;
    _clientId    = buildClientId();
    _statusTopic = "atgenx/" + _userId + "/" + _clientId + "/status";
    _otaTopic    = "atgenx/" + _userId + "/" + _clientId + "/ota";

    Serial.println();
    Serial.println(F("──────────── ATGenX Hub v2.3.0 ────────────"));
    Serial.print(F("  Board     : ")); Serial.println(getBoardType());
    Serial.print(F("  Client ID : ")); Serial.println(_userId);
    Serial.print(F("  Board ID  : ")); Serial.println(_clientId);
    Serial.print(F("  Broker    : ")); Serial.print(_host); Serial.print(':'); Serial.println(_port);
    Serial.println(F("────────────────────────────────────────────"));

    connectWiFi(ssid, password);

    _mqtt.setServer(_host.c_str(), _port);
    _mqtt.setCallback(mqttCallback);
    _mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
    _mqtt.setSocketTimeout(5);
    _mqtt.setBufferSize(MQTT_BUFFER_BYTES);

    for (uint8_t attempt = 1; attempt <= MQTT_BEGIN_TRIES; ++attempt) {
        if (tryConnectMQTT()) return;
        if (attempt < MQTT_BEGIN_TRIES) delay(RETRY_MIN_MS);
    }
    Serial.println(F("[MQTT] Still offline — retrying in the background"));
    _lastAttemptMs = millis();
}

void ATGenX_Hub::loop() {
    serviceConnection();
    if (_mqtt.connected()) _mqtt.loop();

    // Sensors keep sampling while offline (their publish just fails quietly)
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

    // Subscribe now if already connected; onMqttConnected() handles it otherwise
    if (_mqtt.connected()) {
        const String topic = "atgenx/" + _userId + "/" + device.getId() + "/cmd";
        subscribe(topic.c_str());
        device.publishState();
    }

    Serial.print(F("[ATGenX] Output '"));
    Serial.print(device.getId());
    Serial.print(F("'  GPIO "));
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

const char* ATGenX_Hub::getUserId()   const { return _userId.c_str(); }
const char* ATGenX_Hub::getClientId() const { return _clientId.c_str(); }

const char* ATGenX_Hub::getBoardType() const {
#if defined(ESP32)
#   if   CONFIG_IDF_TARGET_ESP32C3
        return "ESP32-C3";
#   elif CONFIG_IDF_TARGET_ESP32C6
        return "ESP32-C6";
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
// Publish / subscribe
// ═══════════════════════════════════════════════════════════════════════════

bool ATGenX_Hub::publish(const char* topic, const char* payload, bool retained) {
    if (!_mqtt.connected()) return false;   // offline: not an error, reconnect is running
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
    Serial.print('"');

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(ssid, pass);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_SEC * 1000UL) {
            Serial.println();
            reportError(ATGenX_Error::WIFI_TIMEOUT, ssid);
            Serial.println(F("[WiFi] Timeout – check the network name/password. Restarting…"));
            delay(200);
            ESP.restart();
        }
        delay(500);
        Serial.print('.');
    }

    Serial.println();
    Serial.print(F("[WiFi] Connected  IP="));
    Serial.print(WiFi.localIP());
    Serial.print(F("  RSSI="));
    Serial.println(WiFi.RSSI());
}

bool ATGenX_Hub::tryConnectMQTT() {
    Serial.print(F("[MQTT] Connecting … "));

    // Last will: the broker marks this board offline if it vanishes
    const bool ok = _mqtt.connect(_clientId.c_str(),
                                  _mqttUser.c_str(),
                                  _mqttPass.c_str(),
                                  _statusTopic.c_str(),
                                  /*willQos=*/1,
                                  /*willRetain=*/true,
                                  "{\"online\":false}");
    if (ok) {
        Serial.println(F("connected"));
        _retryMs = RETRY_MIN_MS;
        onMqttConnected();
        return true;
    }

    const int rc = _mqtt.state();
    Serial.print(F("failed – "));
    Serial.println(mqttStateText(rc));
    // 4/5 = wrong credentials or no free device slot: tell the sketch why
    if (rc == MQTT_CONNECT_BAD_CREDENTIALS || rc == MQTT_CONNECT_UNAUTHORIZED) {
        reportError(ATGenX_Error::MQTT_FAILED, mqttStateText(rc));
    }
    return false;
}

void ATGenX_Hub::serviceConnection() {
    if (_mqtt.connected()) return;

    if (_wasConnected) {
        _wasConnected = false;
        reportError(ATGenX_Error::MQTT_RECONNECT, "connection lost – reconnecting");
        if (_onConnectionChange) _onConnectionChange(false);
        _lastAttemptMs = millis();
        _retryMs = RETRY_MIN_MS;
        return;
    }

    if (WiFi.status() != WL_CONNECTED) return;          // radio is reconnecting
    if (millis() - _lastAttemptMs < _retryMs) return;   // back-off

    _lastAttemptMs = millis();
    if (!tryConnectMQTT()) {
        _retryMs = (_retryMs * 2 > RETRY_MAX_MS) ? RETRY_MAX_MS : _retryMs * 2;
        Serial.print(F("[MQTT] Next try in "));
        Serial.print(_retryMs / 1000);
        Serial.println(F(" s"));
    }
}

void ATGenX_Hub::onMqttConnected() {
    _wasConnected = true;

    // project ids are UUIDs (36 chars) — anything odd is left out rather than breaking the JSON
    char project[48] = "";
    if (g_atgxProjectId && strlen(g_atgxProjectId) < sizeof(project) && !strpbrk(g_atgxProjectId, "\"\\")) {
        strncpy(project, g_atgxProjectId, sizeof(project) - 1);
    }
    char status[240];
    snprintf(status, sizeof(status),
             "{\"online\":true,\"board\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"fw\":%lu,\"project\":\"%s\"}",
             getBoardType(), WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(), (unsigned long)g_atgxFirmwareVersion, project);
    _mqtt.publish(_statusTopic.c_str(), status, /*retained=*/true);
    if (_otaEnabled) subscribe(_otaTopic.c_str());

    for (size_t i = 0; i < _deviceCount; ++i) {
        const String topic = "atgenx/" + _userId + "/" + _devices[i]->getId() + "/cmd";
        subscribe(topic.c_str());
        _devices[i]->publishState();
    }

    // Fresh readings right away so dashboards don't wait a full interval
    for (size_t i = 0; i < _sensorCount; ++i) {
        _sensors[i]->publishNow();
    }

    if (_onConnectionChange) _onConnectionChange(true);
}

void ATGenX_Hub::handleMessage(char* topic, byte* payload, unsigned int len) {
    if (_otaEnabled && _otaTopic.equals(topic)) {
        handleOta(reinterpret_cast<const char*>(payload), len);
        return;
    }

    // Expected: atgenx/<userId>/<deviceId>/cmd
    const String t(topic);
    const String prefix = "atgenx/" + _userId + "/";
    if (!t.startsWith(prefix) || !t.endsWith("/cmd")) return;

    const String deviceId = t.substring(prefix.length(), t.length() - 4);

    for (size_t i = 0; i < _deviceCount; ++i) {
        if (deviceId.equals(_devices[i]->getId())) {
            _devices[i]->handleCommand(reinterpret_cast<const char*>(payload), len);
            return;
        }
    }

    Serial.print(F("[ATGenX] No output named '"));
    Serial.print(deviceId);
    Serial.println(F("' on this board"));
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
    // Same format as v2.1 — existing boards keep their device slot
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

const char* ATGenX_Hub::mqttStateText(int state) {
    switch (state) {
        case MQTT_CONNECTION_TIMEOUT:      return "broker not responding (timeout)";
        case MQTT_CONNECTION_LOST:         return "connection lost";
        case MQTT_CONNECT_FAILED:          return "broker unreachable (check internet / host / port)";
        case MQTT_DISCONNECTED:            return "disconnected";
        case MQTT_CONNECT_BAD_PROTOCOL:    return "bad protocol";
        case MQTT_CONNECT_BAD_CLIENT_ID:   return "client id rejected";
        case MQTT_CONNECT_UNAVAILABLE:     return "broker unavailable";
        case MQTT_CONNECT_BAD_CREDENTIALS: return "wrong MQTT username/password";
        case MQTT_CONNECT_UNAUTHORIZED:    return "not authorised (expired plan or device limit reached)";
        default:                           return "unknown error";
    }
}

void ATGenX_Hub::mqttCallback(char* topic, byte* payload, unsigned int len) {
    if (_instance) _instance->handleMessage(topic, payload, len);
}

// ═══════════════════════════════════════════════════════════════════════════
// Over-the-air updates
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Hub::otaStatus(const char* state, long version, const char* error) {
    char msg[200];
    if (error) snprintf(msg, sizeof(msg), "{\"ota\":\"%s\",\"version\":%ld,\"error\":\"%.120s\"}", state, version, error);
    else       snprintf(msg, sizeof(msg), "{\"ota\":\"%s\",\"version\":%ld}", state, version);
    _mqtt.publish(_statusTopic.c_str(), msg, /*retained=*/false);
    _mqtt.loop();
}

void ATGenX_Hub::handleOta(const char* payload, unsigned int len) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;
    const char* url = doc["url"] | "";
    const long version = doc["version"] | 0L;
    if (!*url) return;
    if (version > 0 && (uint32_t)version == g_atgxFirmwareVersion) {
        otaStatus("ok", version);          // already running it
        return;
    }

    Serial.print(F("[OTA] Updating to v"));
    Serial.println(version);
    otaStatus("downloading", version);
    for (size_t i = 0; i < _deviceCount; ++i) _devices[i]->turnOff();   // outputs off while flashing

    const bool https = strncmp(url, "https://", 8) == 0;
#if defined(ESP32)
    WiFiClient plain;
    WiFiClientSecure secure;
    secure.setInsecure();                  // the MD5 from the cloud still guards the image
    httpUpdate.rebootOnUpdate(false);
    const HTTPUpdateResult r = https ? httpUpdate.update(secure, url) : httpUpdate.update(plain, url);
    const String err = httpUpdate.getLastErrorString();
#elif defined(ESP8266)
    WiFiClient plain;
    BearSSL::WiFiClientSecure secure;
    secure.setInsecure();
    ESPhttpUpdate.rebootOnUpdate(false);
    const HTTPUpdateResult r = https ? ESPhttpUpdate.update(secure, url) : ESPhttpUpdate.update(plain, url);
    const String err = ESPhttpUpdate.getLastErrorString();
#endif

    if (r == HTTP_UPDATE_OK) {
        Serial.println(F("[OTA] Done — restarting"));
        otaStatus("ok", version);
        delay(300);
        ESP.restart();
    } else {
        Serial.print(F("[OTA] Failed: "));
        Serial.println(err);
        otaStatus("failed", version, err.c_str());
    }
}
