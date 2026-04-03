/**
 * @file    ATGenX_DHT.h
 * @brief   DHT11 / DHT22 temperature & humidity sensor for ATGenX
 * @version 1.0.0
 *
 * Requires the "DHT sensor library" by Adafruit (Library Manager).
 *
 * Published payload (JSON, /reading topic)
 * ─────────────────────────────────────────
 * @code
 * { "tempC": 24.5, "humidity": 61.2, "ts": 12345 }
 * @endcode
 *
 * Usage
 * ──────
 * @code
 *   ATGenX_DHT dht(4, "dht1");          // pin 4, DHT22 by default
 *   // or:
 *   ATGenX_DHT dht(4, "dht1", DHT11);   // DHT11 variant
 *
 *   void setup() {
 *       hub.attachSensor(dht);
 *       dht.begin();
 *   }
 * @endcode
 */

#pragma once
#include "ATGenX_Sensor.h"
#include <DHT.h>

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_DHT
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_DHT : public ATGenX_Sensor {
public:

    /**
     * @param pin         GPIO pin connected to DHT data line.
     * @param sensorId    Unique ID for MQTT topic (e.g. "dht1").
     * @param dhtType     DHT11 or DHT22 (default DHT22).
     * @param intervalMs  Polling interval; DHT22 needs ≥ 2000 ms.
     */
    ATGenX_DHT(uint8_t     pin,
               const char* sensorId,
               uint8_t     dhtType    = DHT22,
               uint32_t    intervalMs = 5000)
        : ATGenX_Sensor(sensorId, intervalMs),
          _dht(pin, dhtType)
    {}

    /** @brief Call once in setup() before hub.begin(). */
    void begin() { _dht.begin(); }

protected:

    bool readAndBuildPayload(char* buf, size_t bufSize) override {
        const float t = _dht.readTemperature();
        const float h = _dht.readHumidity();

        if (isnan(t) || isnan(h)) return false;

        snprintf(buf, bufSize,
                 "{\"tempC\":%.1f,\"humidity\":%.1f,\"ts\":%lu}",
                 t, h, millis());
        return true;
    }

private:
    DHT _dht;
};