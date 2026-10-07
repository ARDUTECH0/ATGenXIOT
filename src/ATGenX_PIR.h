/**
 * @file    ATGenX_PIR.h
 * @brief   PIR motion sensor for ATGenX
 * @version 1.1.0
 *
 * Operates in two modes selected at construction:
 *
 *   Polling      – checks the pin every intervalMs; publishes only when the
 *                  motion state has changed (delta publishing).
 *   Event-driven – attach an interrupt to the pin and call publishNow()
 *                  from the ISR (set intervalMs = 0 to disable polling).
 *
 * Delta publishing
 * ─────────────────
 * ATGenX_PIR owns its own delta check on the raw digital value rather than
 * delegating to the base-class payload comparison.  This avoids false
 * "no change" results caused by the ever-changing "ts" field in the JSON
 * payload — only a genuine 0→1 or 1→0 transition triggers a publish.
 *
 * Published payload (JSON, /reading topic)
 * ─────────────────────────────────────────
 * @code
 * { "motion": 1, "ts": 12345 }     // motion detected
 * { "motion": 0, "ts": 12367 }     // no motion
 * @endcode
 *
 * Usage – polling mode
 * ─────────────────────
 * @code
 *   ATGenX_PIR pir(13, "pir1");   // poll every 500 ms (default)
 *   hub.attachSensor(pir);
 *   pir.begin();
 * @endcode
 *
 * Usage – event-driven mode
 * ──────────────────────────
 * @code
 *   ATGenX_PIR pir(13, "pir1", 0);   // intervalMs = 0 → no polling
 *   ATGenX_PIR* pirPtr = &pir;
 *
 *   void IRAM_ATTR onMotion() { pirPtr->publishNow(); }
 *
 *   void setup() {
 *       hub.attachSensor(pir);
 *       pir.begin();
 *       attachInterrupt(digitalPinToInterrupt(13), onMotion, CHANGE);
 *   }
 * @endcode
 */

#pragma once
#include "ATGenX_Sensor.h"

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_PIR
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_PIR : public ATGenX_Sensor {
public:

    /**
     * @param pin         GPIO pin connected to PIR OUT.
     * @param sensorId    Unique ID for MQTT topic (e.g. "pir1").
     * @param intervalMs  Polling interval; 0 = event-driven only.
     */
    ATGenX_PIR(uint8_t     pin,
               const char* sensorId,
               uint32_t    intervalMs = 500)
        : ATGenX_Sensor(sensorId, intervalMs),
          _pin(pin),
          _lastMotion(-1)   // -1 = never published; guarantees first read always fires
    {}

    /** @brief Call once in setup(). */
    void begin() { pinMode(_pin, INPUT); }

protected:

    /**
     * @brief Reads the PIR pin and builds a JSON payload.
     *
     * Returns false (skip publish) when the motion state has not changed
     * since the last successful publish.  The "ts" timestamp is written
     * only after a state change is confirmed, so it always reflects a real
     * transition event rather than a periodic heartbeat.
     */
    bool readAndBuildPayload(char* buf, size_t bufSize) override {
        const int motion = digitalRead(_pin);

        // ── Delta check — subclass owns this because the payload contains
        //    "ts": millis() which would defeat the base-class string compare.
        if (motion == _lastMotion && !isForced()) {
            return false;   // no state change → skip publish
        }

        _lastMotion = motion;
        snprintf(buf, bufSize,
                 "{\"motion\":%d,\"ts\":%lu}",
                 motion, millis());
        return true;
    }

private:
    uint8_t _pin;
    int     _lastMotion;   ///< Last published motion value; -1 = never published.
};