/**
 * @file    ATGenX_Ultrasonic.h
 * @brief   HC-SR04 ultrasonic distance sensor for ATGenX
 * @version 1.0.0
 *
 * Uses a blocking pulseIn() read (< 30 ms).  If non-blocking operation
 * is needed, implement it in a custom subclass of ATGenX_Sensor.
 *
 * Published payload (JSON, /reading topic)
 * ─────────────────────────────────────────
 * @code
 * { "value": 23.5, "unit": "cm", "ts": 12345 }
 * @endcode
 *
 *   value  – distance in centimetres
 *   unit   – always "cm"
 *
 * Usage
 * ──────
 * @code
 *   ATGenX_Ultrasonic us(12, 14, "us1");   // trig=12, echo=14
 *   hub.attachSensor(us);
 *   us.begin();
 * @endcode
 *
 * Wiring (HC-SR04)
 * ─────────────────
 *   VCC  → 5 V  (or 3.3 V tolerant copy)
 *   GND  → GND
 *   TRIG → trigPin
 *   ECHO → echoPin  (use a 1 kΩ / 2 kΩ voltage divider for 3.3 V GPIOs)
 */

#pragma once
#include "ATGenX_Sensor.h"

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Ultrasonic
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Ultrasonic : public ATGenX_Sensor {
public:

    /** Maximum plausible echo travel time in µs (~4 m round-trip). */
    static constexpr unsigned long MAX_ECHO_US = 23200UL;

    /**
     * @param trigPin     GPIO pin connected to HC-SR04 TRIG.
     * @param echoPin     GPIO pin connected to HC-SR04 ECHO.
     * @param sensorId    Unique ID for MQTT topic (e.g. "us1").
     * @param intervalMs  Polling interval in ms.
     */
    ATGenX_Ultrasonic(uint8_t     trigPin,
                      uint8_t     echoPin,
                      const char* sensorId,
                      uint32_t    intervalMs = 1000)
        : ATGenX_Sensor(sensorId, intervalMs),
          _trig(trigPin),
          _echo(echoPin)
    {}

    /** @brief Call once in setup(). */
    void begin() {
        pinMode(_trig, OUTPUT);
        pinMode(_echo, INPUT);
        digitalWrite(_trig, LOW);
    }

protected:

    bool readAndBuildPayload(char* buf, size_t bufSize) override {
        const float cm = measureCm();
        if (cm < 0.0f) return false;   // timeout / out of range

        snprintf(buf, bufSize,
                 "{\"value\":%.1f,\"unit\":\"cm\",\"ts\":%lu}",
                 cm, millis());
        return true;
    }

private:

    uint8_t _trig;
    uint8_t _echo;

    float measureCm() const {
        // Trigger pulse
        digitalWrite(_trig, LOW);
        delayMicroseconds(2);
        digitalWrite(_trig, HIGH);
        delayMicroseconds(10);
        digitalWrite(_trig, LOW);

        // Measure echo
        const unsigned long duration = pulseIn(_echo, HIGH, MAX_ECHO_US);
        if (duration == 0) return -1.0f;   // timeout

        // Speed of sound: 343 m/s → 0.0343 cm/µs → divide by 2 (round-trip)
        return duration * 0.01715f;
    }
};