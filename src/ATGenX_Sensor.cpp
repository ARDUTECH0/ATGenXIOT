/**
 * @file    ATGenX_Sensor.cpp
 * @brief   ATGenX_Sensor implementation
 * @version 1.1.0
 */

#include "ATGenX_Sensor.h"
#include "ATGenX_Hub.h"

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

ATGenX_Sensor::ATGenX_Sensor(const char* sensorId, uint32_t intervalMs)
    : _id(sensorId),
      _hub(nullptr),
      _intervalMs(intervalMs),
      _lastReadMs(0),
      _lastPayload()
{
}

// ═══════════════════════════════════════════════════════════════════════════
// Internal – hub binding
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Sensor::attachTo(ATGenX_Hub* hub) {
    _hub          = hub;
    _fullPath     = String(hub->getUserId()) + "/" + _id;
    _topicReading = "atgenx/" + _fullPath + "/reading";

    Serial.print(F("[ATGenX] Sensor '"));
    Serial.print(_id);
    Serial.print(F("' → topic: "));
    Serial.println(_topicReading);
}

// ═══════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Sensor::loop() {
    if (!_hub || _intervalMs == 0) return;

    const uint32_t now     = millis();
    const uint32_t elapsed = (now >= _lastReadMs)
        ? (now - _lastReadMs)
        : (0xFFFFFFFFUL - _lastReadMs + now + 1UL);   // millis() rollover

    if (elapsed >= _intervalMs) {
        doRead();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Control
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Sensor::publishNow() {
    doRead();
}

void ATGenX_Sensor::setInterval(uint32_t intervalMs) {
    _intervalMs = intervalMs;
}

// ═══════════════════════════════════════════════════════════════════════════
// Query
// ═══════════════════════════════════════════════════════════════════════════

const char* ATGenX_Sensor::getId()       const { return _id.c_str();           }
const char* ATGenX_Sensor::getFullPath() const { return _fullPath.c_str();     }
const char* ATGenX_Sensor::getTopic()    const { return _topicReading.c_str(); }

// ═══════════════════════════════════════════════════════════════════════════
// Private helpers
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Sensor::doRead() {
    _lastReadMs = millis();

    char buf[256];
    buf[0] = '\0';

    // readAndBuildPayload() returns false either on a hardware error
    // OR when the subclass already detected no meaningful change.
    if (!readAndBuildPayload(buf, sizeof(buf))) {
        Serial.print(F("[ATGenX] Sensor '"));
        Serial.print(_id);
        Serial.println(F("' – no change or read failed, skipping publish"));
        return;
    }

    // ── Delta check ────────────────────────────────────────────────────────
    // Skip publish when the payload is identical to the last one sent.
    // Subclasses that embed a timestamp should handle their own comparison
    // on the meaningful fields and return false above instead.
    if (_lastPayload == buf) {
        Serial.print(F("[ATGenX] Sensor '"));
        Serial.print(_id);
        Serial.println(F("' – payload unchanged, skipping publish"));
        return;
    }

    _lastPayload = buf;   // cache before publish
    publishPayload(buf);
}

void ATGenX_Sensor::publishPayload(const char* payload) {
    if (!_hub) return;

    const bool ok = _hub->publish(_topicReading.c_str(), payload, /*retained=*/false);

    Serial.print(F("[ATGenX] Sensor '"));
    Serial.print(_id);
    Serial.print(F("' → "));
    Serial.print(ok ? F("OK") : F("FAIL"));
    Serial.print(F("  "));
    Serial.println(payload);
}