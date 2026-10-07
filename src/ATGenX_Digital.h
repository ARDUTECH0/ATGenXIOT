/**
 * @file    ATGenX_Digital.h
 * @brief   Any ON/OFF input (push button, flame, reed, IR obstacle, tilt,
 *          touch, limit switch, sound DO …) for the ATGenX IoT platform
 * @version 2.2.0
 *
 * Payload  (atgenx/<clientId>/<sensorId>/reading):  {"state":0|1}
 * Published on every (debounced) change — dashboards and "turns ON/OFF"
 * automations react instantly.
 *
 * @code
 *   ATGenX_Digital door(27, "door", true);   // reed switch to GND, pulled up
 *   …
 *   hub.attachSensor(door);
 *   door.begin();
 * @endcode
 */

#pragma once
#include "ATGenX_Sensor.h"

class ATGenX_Digital : public ATGenX_Sensor {
public:

    /**
     * @param pin         GPIO.
     * @param sensorId    Unique ID for the MQTT topic (e.g. "door").
     * @param activeLow   true when the input reads LOW while active (buttons
     *                    and switches wired to GND, most IR/flame modules).
     *                    Enables the internal pull-up.
     * @param debounceMs  Ignore changes shorter than this.
     */
    ATGenX_Digital(uint8_t     pin,
                   const char* sensorId,
                   bool        activeLow  = true,
                   uint16_t    debounceMs = 30)
        : ATGenX_Sensor(sensorId, 10),
          _pin(pin),
          _activeLow(activeLow),
          _debounceMs(debounceMs),
          _published(-1),
          _candidate(-1),
          _since(0)
    {}

    /** @brief Call once in setup(). */
    void begin() { pinMode(_pin, _activeLow ? INPUT_PULLUP : INPUT); }

    /** @brief Current (debounced) state. */
    bool isActive() const { return _published == 1; }

protected:

    bool readAndBuildPayload(char* buf, size_t bufSize) override {
        const int raw = digitalRead(_pin);
        const int on  = _activeLow ? (raw == LOW) : (raw == HIGH);
        const uint32_t now = millis();

        if (on != _candidate) { _candidate = on; _since = now; }
        const bool stable = now - _since >= _debounceMs;
        if (!isForced() && (!stable || _candidate == _published)) return false;

        _published = stable ? _candidate : (_published < 0 ? on : _published);
        snprintf(buf, bufSize, "{\"state\":%d}", _published);
        return true;
    }

private:
    uint8_t  _pin;
    bool     _activeLow;
    uint16_t _debounceMs;
    int      _published;
    int      _candidate;
    uint32_t _since;
};
