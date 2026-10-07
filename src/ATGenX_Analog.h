/**
 * @file    ATGenX_Analog.h
 * @brief   Any analog sensor (potentiometer, soil moisture, gas MQ-x,
 *          water level, rain …) for the ATGenX IoT platform
 * @version 2.2.0
 *
 * Payload  (atgenx/<clientId>/<sensorId>/reading):
 *   {"value":<raw ADC>,"percent":<0-100>}
 *
 * A reading is published when it moves by at least `minChange` raw steps
 * (filters ADC noise), and at least every `heartbeatMs` so dashboards and
 * history stay fresh.
 *
 * @code
 *   ATGenX_Analog soil(34, "soil");            // ESP32 ADC1 pin
 *   …
 *   hub.attachSensor(soil);
 *   soil.begin();
 * @endcode
 */

#pragma once
#include "ATGenX_Sensor.h"

class ATGenX_Analog : public ATGenX_Sensor {
public:

    /**
     * @param pin          Analog-capable GPIO (ESP32: use ADC1 pins 32-39 with Wi-Fi on).
     * @param sensorId     Unique ID for the MQTT topic (e.g. "soil").
     * @param intervalMs   Sampling period in ms.
     * @param minChange    Raw steps the value must move before it is re-sent.
     * @param heartbeatMs  Re-send at least this often even if unchanged (0 = never).
     */
    ATGenX_Analog(uint8_t     pin,
                  const char* sensorId,
                  uint32_t    intervalMs  = 1000,
                  uint16_t    minChange   = 20,
                  uint32_t    heartbeatMs = 30000)
        : ATGenX_Sensor(sensorId, intervalMs),
          _pin(pin),
          _minChange(minChange),
          _heartbeatMs(heartbeatMs),
          _last(-1),
          _lastSentMs(0)
    {}

    /** @brief Call once in setup(). */
    void begin() { pinMode(_pin, INPUT); }

    /** @brief Latest raw reading (−1 before the first sample). */
    int raw() const { return _last; }

protected:

    bool readAndBuildPayload(char* buf, size_t bufSize) override {
#if defined(ESP8266)
        static constexpr int ADC_MAX = 1023;
#else
        static constexpr int ADC_MAX = 4095;
#endif
        const int v = analogRead(_pin);
        const uint32_t now = millis();
        const bool moved = _last < 0 || abs(v - _last) >= _minChange;
        const bool stale = _heartbeatMs && (now - _lastSentMs >= _heartbeatMs);
        if (!moved && !stale && !isForced()) return false;

        _last = v;
        _lastSentMs = now;
        const int percent = (int)((v * 100L + ADC_MAX / 2) / ADC_MAX);
        snprintf(buf, bufSize, "{\"value\":%d,\"percent\":%d}", v, percent);
        return true;
    }

private:
    uint8_t  _pin;
    uint16_t _minChange;
    uint32_t _heartbeatMs;
    int      _last;
    uint32_t _lastSentMs;
};
