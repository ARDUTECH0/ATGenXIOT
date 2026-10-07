/**
 * @file    ATGenX_Sensor.cpp
 * @brief   ATGenX_Sensor implementation
 * @version 2.2.0
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
    _force = true;
    doRead();
    _force = false;
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

    // false = hardware error, or the subclass saw no meaningful change
    if (!readAndBuildPayload(buf, sizeof(buf))) {
#if ATGENX_DEBUG
        Serial.print(F("[ATGenX] Sensor '"));
        Serial.print(_id);
        Serial.println(F("' – no change or read failed"));
#endif
        return;
    }

    // Identical payload → nothing new to say (unless forced)
    if (!_force && _lastPayload == buf) return;

    _lastPayload = buf;
    publishPayload(buf);
}

void ATGenX_Sensor::publishPayload(const char* payload) {
    if (!_hub) return;

    const bool ok = _hub->publish(_topicReading.c_str(), payload, /*retained=*/false);
    if (!ok) _lastPayload = "";   // offline: send it again once we're back

#if ATGENX_DEBUG
    Serial.print(F("[ATGenX] Sensor '"));
    Serial.print(_id);
    Serial.print(F("' → "));
    Serial.print(ok ? F("OK  ") : F("FAIL  "));
    Serial.println(payload);
#endif
}
