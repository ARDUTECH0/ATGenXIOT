/**
 * @file    ATGenX_Device.cpp
 * @brief   ATGenX_Device implementation
 * @version 2.0.0
 */

#include "ATGenX_Device.h"
#include "ATGenX_Hub.h"

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

ATGenX_Device::ATGenX_Device(uint8_t     pin,
                             const char* deviceId,
                             bool        activeLow)
    : _pin(pin),
      _activeLow(activeLow),
      _state(false),
      _id(deviceId),
      _hub(nullptr),
      _onStateChange(nullptr)
{
    pinMode(_pin, OUTPUT);
    applyState(false);  // Ensure relay starts OFF regardless of board state
}

// ═══════════════════════════════════════════════════════════════════════════
// Internal – hub binding
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Device::attachTo(ATGenX_Hub* hub) {
    _hub = hub;

    _fullPath   = String(hub->getUserId()) + "/" + _id;
    _topicState = "atgenx/" + _fullPath + "/state";
}

// ═══════════════════════════════════════════════════════════════════════════
// Control API
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Device::turnOn()  { applyState(true);        }
void ATGenX_Device::turnOff() { applyState(false);       }
void ATGenX_Device::toggle()  { applyState(!_state);     }

// ═══════════════════════════════════════════════════════════════════════════
// Query API
// ═══════════════════════════════════════════════════════════════════════════

bool        ATGenX_Device::isOn()       const { return _state;             }
uint8_t     ATGenX_Device::getPin()     const { return _pin;               }
const char* ATGenX_Device::getId()      const { return _id.c_str();        }
const char* ATGenX_Device::getFullPath() const { return _fullPath.c_str(); }

// ═══════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Device::onStateChange(void (*cb)(bool newState)) {
    _onStateChange = cb;
}

// ═══════════════════════════════════════════════════════════════════════════
// Internal – MQTT command handler
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Device::handleCommand(const char* payload, unsigned int len) {
    // Guard against oversized payloads
    constexpr size_t MAX_CMD = 64;
    if (len == 0 || len > MAX_CMD) return;

    char buf[MAX_CMD + 1];
    memcpy(buf, payload, len);
    buf[len] = '\0';

    int8_t cmd = parseCommand(buf);

    if      (cmd ==  1) { applyState(true);   }
    else if (cmd ==  0) { applyState(false);  }
    else if (cmd == -2) { toggle();           }   // explicit "toggle"
    else {
        Serial.print(F("[ATGenX] Device '"));
        Serial.print(_id);
        Serial.print(F("' – unrecognised command: "));
        Serial.println(buf);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Internal – state publication
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Device::publishState() const {
    if (!_hub) return;

    char payload[160];
    snprintf(payload, sizeof(payload),
             "{\"state\":%d,\"label\":\"%s\",\"pin\":%d,\"id\":\"%s\",\"ts\":%lu}",
             _state ? 1 : 0,
             _state ? "ON" : "OFF",
             _pin,
             _id.c_str(),
             millis());

    _hub->publish(_topicState.c_str(), payload, /*retained=*/true);
}

// ═══════════════════════════════════════════════════════════════════════════
// Private helpers
// ═══════════════════════════════════════════════════════════════════════════

void ATGenX_Device::applyState(bool newState) {
    _state = newState;

    const uint8_t level = _activeLow
        ? (newState ? LOW : HIGH)
        : (newState ? HIGH : LOW);

    digitalWrite(_pin, level);

    Serial.print(F("[ATGenX] '"));
    Serial.print(_id);
    Serial.print(F("' → "));
    Serial.print(newState ? F("ON") : F("OFF"));
    Serial.print(F("  (GPIO"));
    Serial.print(_pin);
    Serial.print(F(" = "));
    Serial.print(level == HIGH ? F("HIGH") : F("LOW"));
    Serial.println(')');

    if (_onStateChange) {
        _onStateChange(newState);
    }

    publishState();
}

// ─── Command parser ──────────────────────────────────────────────────────────
//  Returns:  1  → ON
//            0  → OFF
//           -1  → unrecognised
//           -2  → toggle

int8_t ATGenX_Device::parseCommand(const char* raw) const {
    // ── JSON path ──────────────────────────────────────────────────────────
    if (raw[0] == '{') {
        // Lightweight field search — avoids heap allocation of a JSON library
        const char* p = strstr(raw, "\"state\"");
        if (!p) return -1;
        p += 7;  // skip "state"

        // skip whitespace and colon
        while (*p == ' ' || *p == ':') ++p;

        // Numeric value
        if (*p == '1') return  1;
        if (*p == '0') return  0;

        // String value
        if (*p == '"') {
            ++p;
            if (strncasecmp(p, "on",     2) == 0) return  1;
            if (strncasecmp(p, "off",    3) == 0) return  0;
            if (strncasecmp(p, "toggle", 6) == 0) return -2;
        }
        return -1;
    }

    // ── Plain-text path ───────────────────────────────────────────────────
    // Work on a lowercase copy without heap allocation
    char lower[65];
    size_t i = 0;
    while (raw[i] && i < 64) { lower[i] = tolower((unsigned char)raw[i]); ++i; }
    lower[i] = '\0';

    if (strcmp(lower, "1")      == 0) return  1;
    if (strcmp(lower, "on")     == 0) return  1;
    if (strcmp(lower, "true")   == 0) return  1;
    if (strcmp(lower, "0")      == 0) return  0;
    if (strcmp(lower, "off")    == 0) return  0;
    if (strcmp(lower, "false")  == 0) return  0;
    if (strcmp(lower, "toggle") == 0) return -2;

    return -1;
}