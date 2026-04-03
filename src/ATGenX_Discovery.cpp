/**
 * @file    ATGenX_Discovery.cpp
 * @brief   ATGenX_Discovery implementation
 * @version 1.1.2  (ArduinoJson v7 compatible)
 */

#include "ATGenX_Discovery.h"
#include <ArduinoJson.h>

#if defined(ESP32)
#   include <WiFi.h>
#elif defined(ESP8266)
#   include <ESP8266WiFi.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

ATGenX_Discovery::ATGenX_Discovery(ATGenX_Hub& hub, uint32_t intervalMs)
    : _hub(hub),
      _intervalMs(max(intervalMs, MIN_INTERVAL_MS)),
      _lastAnnounceMs(0)
{
}

// ═══════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Discovery::begin() {
    _topic  = "atgenx/";
    _topic += _hub.getUserId();
    _topic += "/discovery";

    Serial.print(F("[Discovery] Topic: "));
    Serial.println(_topic);

    announce();
}

void ATGenX_Discovery::loop() {
    if (!_hub.isConnected()) return;

    const uint32_t now     = millis();
    const uint32_t elapsed = (now >= _lastAnnounceMs)
        ? (now - _lastAnnounceMs)
        : (0xFFFFFFFFUL - _lastAnnounceMs + now + 1UL);

    if (elapsed >= _intervalMs) {
        announce();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Control
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Discovery::announce() {
    if (!_hub.isConnected()) return;
    publishAnnouncement();
    _lastAnnounceMs = millis();
}

void ATGenX_Discovery::setInterval(uint32_t intervalMs) {
    _intervalMs = max(intervalMs, MIN_INTERVAL_MS);
}

const char* ATGenX_Discovery::getTopic() const {
    return _topic.c_str();
}

// ═══════════════════════════════════════════════════════════════════════════
// Private helpers
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Discovery::publishAnnouncement() const {
    // ArduinoJson v7: JsonDocument is allocated on the heap automatically
    JsonDocument doc;

    doc["userId"]  = _hub.getUserId();
    doc["board"]   = _hub.getBoardType();
    doc["ip"]      = WiFi.localIP().toString();
    doc["rssi"]    = (int)WiFi.RSSI();
    doc["uptime"]  = millis() / 1000UL;
    doc["status"]  = "online";

    JsonArray devices = doc["devices"].to<JsonArray>();

    for (size_t i = 0; i < _hub.deviceCount(); ++i) {
        const ATGenX_Device* dev = _hub.getDevice(i);
        if (!dev) continue;

        JsonObject obj = devices.add<JsonObject>();
        obj["id"]    = dev->getId();
        obj["pin"]   = dev->getPin();
        obj["state"] = dev->isOn() ? 1 : 0;
    }

    char payload[1024];
    const size_t written = serializeJson(doc, payload, sizeof(payload));

    if (written == 0) {
        Serial.println(F("[Discovery] Serialisation failed"));
        return;
    }

    const bool ok = _hub.publish(_topic.c_str(), payload, /*retained=*/true);

    Serial.print(F("[Discovery] Announced  "));
    Serial.print(ok ? F("OK") : F("FAIL"));
    Serial.print(F("  bytes="));
    Serial.print(written);
    Serial.print(F("  devices="));
    Serial.print(_hub.deviceCount());
    Serial.print(F("  rssi="));
    Serial.println(WiFi.RSSI());
}