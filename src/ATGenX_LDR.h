/**
 * @file    ATGenX_LDR.h
 * @brief   LDR (light-dependent resistor) analog sensor for ATGenX
 * @version 1.0.0
 *
 * Reads an analog pin and optionally maps the raw ADC value to a
 * 0–100 lux-percentage scale.
 *
 * Published payload (JSON, /reading topic)
 * ─────────────────────────────────────────
 * @code
 * { "analog": 2047, "percent": 50, "ts": 12345 }
 * @endcode
 *
 *   analog   – raw ADC value   (0 … ADC_MAX)
 *   percent  – brightness %    (0 = dark, 100 = bright)
 *
 * Usage
 * ──────
 * @code
 *   ATGenX_LDR ldr(34, "ldr1");        // 12-bit ADC on ESP32 (default)
 *   // or:
 *   ATGenX_LDR ldr(A0, "ldr1", 1023);  // 10-bit ADC on ESP8266
 *
 *   hub.attachSensor(ldr);
 *   ldr.begin();
 * @endcode
 */

#pragma once
#include "ATGenX_Sensor.h"

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_LDR
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_LDR : public ATGenX_Sensor {
public:

    /**
     * @param pin         Analog-capable GPIO pin.
     * @param sensorId    Unique ID for MQTT topic (e.g. "ldr1").
     * @param adcMax      Full-scale ADC value; 4095 for 12-bit (ESP32 default),
     *                    1023 for 10-bit (ESP8266 / analogReadResolution(10)).
     * @param intervalMs  Polling interval in ms.
     */
    ATGenX_LDR(uint8_t     pin,
               const char* sensorId,
               uint16_t    adcMax     = 4095,
               uint32_t    intervalMs = 2000)
        : ATGenX_Sensor(sensorId, intervalMs),
          _pin(pin),
          _adcMax(adcMax)
    {}

    /** @brief Call once in setup() (no-op on most boards, here for symmetry). */
    void begin() { pinMode(_pin, INPUT); }

protected:

    bool readAndBuildPayload(char* buf, size_t bufSize) override {
        const int  raw     = analogRead(_pin);
        const int  percent = static_cast<int>(
            (static_cast<float>(raw) / _adcMax) * 100.0f + 0.5f);

        snprintf(buf, bufSize,
                 "{\"analog\":%d,\"percent\":%d,\"ts\":%lu}",
                 raw, percent, millis());
        return true;
    }

private:
    uint8_t  _pin;
    uint16_t _adcMax;
};