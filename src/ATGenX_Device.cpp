/**
 * @file    ATGenX_Device.cpp
 * @brief   ATGenX_Device implementation
 * @version 2.2.0
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

    setupPin();

    _fullPath   = String(hub->getUserId()) + "/" + _id;
    _topicState = "atgenx/" + _fullPath + "/state";
}

// Constructors of global objects can run before the core is ready on some
// boards — set the pin up again once the hub takes the device.
void ATGenX_Device::setupPin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, _activeLow ? (_state ? LOW : HIGH) : (_state ? HIGH : LOW));
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
    // Dashboard / cloud commands are small JSON objects (~60–90 bytes)
    constexpr size_t MAX_CMD = 255;
    if (len == 0 || len > MAX_CMD) return;

    char buf[MAX_CMD + 1];
    memcpy(buf, payload, len);
    buf[len] = '\0';

    const int8_t cmd = parseCommand(buf);

    if      (cmd ==  1) { applyState(true);  }
    else if (cmd ==  0) { applyState(false); }
    else if (cmd == -2) { toggle();          }
    else if (cmd == -3) { publishState();    }   // {"cmd":"getState"}
    else {
        Serial.print(F("[ATGenX] '"));
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
//  Returns:  1  → ON          0  → OFF
//           -2  → toggle     -3  → report state
//           -1  → unrecognised
//
//  Accepts  {"state":1|0|true|false|"on"|"off"|"toggle"}
//           {"cmd":"on"|"off"|"toggle"|"getState"}
//           plain  1 / 0 / on / off / true / false / toggle

namespace {
    // Pointer to the value of "key" in a flat JSON object, or nullptr.
    const char* jsonValue(const char* raw, const char* key) {
        char pattern[24];
        snprintf(pattern, sizeof(pattern), "\"%s\"", key);
        const char* p = strstr(raw, pattern);
        if (!p) return nullptr;
        p += strlen(pattern);
        while (*p == ' ' || *p == '\t') ++p;
        if (*p != ':') return nullptr;
        ++p;
        while (*p == ' ' || *p == '\t') ++p;
        return p;
    }

    int8_t wordToCommand(const char* p) {
        if (*p == '"') ++p;
        if (strncasecmp(p, "getstate", 8) == 0) return -3;
        if (strncasecmp(p, "toggle",   6) == 0) return -2;
        if (strncasecmp(p, "true",     4) == 0) return  1;
        if (strncasecmp(p, "false",    5) == 0) return  0;
        if (strncasecmp(p, "on",       2) == 0) return  1;
        if (strncasecmp(p, "off",      3) == 0) return  0;
        if (*p == '1') return 1;
        if (*p == '0') return 0;
        return -1;
    }
}

int8_t ATGenX_Device::parseCommand(const char* raw) const {
    while (*raw == ' ' || *raw == '\r' || *raw == '\n') ++raw;

    if (raw[0] == '{') {
        if (const char* v = jsonValue(raw, "state")) return wordToCommand(v);
        if (const char* v = jsonValue(raw, "cmd"))   return wordToCommand(v);
        return -1;
    }

    // Plain text must be the whole word ("on", not "only")
    const int8_t c = wordToCommand(raw);
    if (c == -1) return -1;
    const size_t n = strlen(raw);
    static const char* const words[] = { "1", "0", "on", "off", "true", "false", "toggle", "getstate" };
    for (const char* w : words) {
        if (n == strlen(w) && strncasecmp(raw, w, n) == 0) return c;
    }
    return -1;
}
