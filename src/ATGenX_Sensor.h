/**
 * @file    ATGenX_Sensor.h
 * @brief   Abstract sensor base class for the ATGenX IoT platform
 * @version 1.1.0
 *
 * All read-only sensor types inherit from ATGenX_Sensor and override
 * readAndBuildPayload() only.  The base class owns the timing loop,
 * MQTT publishing, and topic construction — the subclass knows nothing
 * about MQTT.
 *
 * Delta publishing
 * ─────────────────
 *   doRead() compares the new payload against _lastPayload before
 *   publishing.  If the payload is identical the publish is skipped,
 *   reducing broker load.  Subclasses that embed a timestamp (or any
 *   field that changes every read) should perform the comparison
 *   themselves on the meaningful value and return false from
 *   readAndBuildPayload() when nothing has changed.
 *
 * Topic convention
 * ─────────────────
 *   Reading : atgenx/<userId>/<sensorId>/reading
 *
 * Timing modes
 * ─────────────
 *   Polling      – readAndBuildPayload() called every intervalMs (default).
 *   Event-driven – call publishNow() from an ISR or external trigger.
 *
 * Adding a new sensor
 * ────────────────────
 * @code
 *   class ATGenX_MySensor : public ATGenX_Sensor {
 *   public:
 *       ATGenX_MySensor(uint8_t pin, const char* id)
 *           : ATGenX_Sensor(id, 2000), _pin(pin) {}
 *
 *       void begin() { pinMode(_pin, INPUT); }
 *
 *   protected:
 *       bool readAndBuildPayload(char* buf, size_t sz) override {
 *           int v = analogRead(_pin);
 *           snprintf(buf, sz, "{\"value\":%d}", v);
 *           return true;
 *       }
 *   private:
 *       uint8_t _pin;
 *   };
 * @endcode
 */

#pragma once
#include <Arduino.h>

class ATGenX_Hub;   // Forward declaration

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Sensor
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Sensor {
public:

    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @param sensorId    Unique ID used in MQTT topic (e.g. "dht1").
     * @param intervalMs  Polling period in ms.  0 = event-driven only.
     */
    explicit ATGenX_Sensor(const char* sensorId,
                           uint32_t    intervalMs = 5000);

    virtual ~ATGenX_Sensor() = default;

    // ───────────────────────────────────────────────────────────────────────
    // Lifecycle — called by ATGenX_Hub; not for end-user use
    // ───────────────────────────────────────────────────────────────────────

    /** @internal Binds this sensor to its parent hub; builds topic path. */
    void attachTo(ATGenX_Hub* hub);

    /**
     * @internal Must be called every loop() iteration (hub does this).
     *           Fires readAndBuildPayload() when the interval has elapsed.
     */
    void loop();

    // ───────────────────────────────────────────────────────────────────────
    // Control
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Force an immediate reading + publish regardless of the timer.
     *        Safe to call from user code or an ISR (keep ISR body minimal).
     */
    void publishNow();

    /**
     * @brief Change the polling interval at runtime.
     * @param intervalMs  0 = pause polling; non-zero = new period.
     */
    void setInterval(uint32_t intervalMs);

    // ───────────────────────────────────────────────────────────────────────
    // Query
    // ───────────────────────────────────────────────────────────────────────

    /** @brief Returns the sensor identifier supplied at construction. */
    const char* getId()       const;

    /**
     * @brief Returns the full MQTT path segment (userId/sensorId).
     *        Only valid after attachTo() has been called.
     */
    const char* getFullPath() const;

    /** @brief Returns the full MQTT topic this sensor publishes to. */
    const char* getTopic()    const;

protected:

    // ───────────────────────────────────────────────────────────────────────
    // Override point — subclasses implement this
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Read the hardware and write a JSON payload into buf.
     *
     * @param buf     Output buffer; guaranteed to be at least bufSize bytes.
     * @param bufSize Size of buf in bytes.
     * @return true   Payload written — will be published (subject to delta
     *                check against _lastPayload in the base class).
     * @return false  Read failed OR no meaningful change detected (subclass
     *                handled its own delta check) — publish is skipped.
     */
    virtual bool readAndBuildPayload(char* buf, size_t bufSize) = 0;

private:

    String      _id;
    String      _fullPath;
    String      _topicReading;

    ATGenX_Hub* _hub;
    uint32_t    _intervalMs;
    uint32_t    _lastReadMs;

    /**
     * @brief Cache of the last successfully published payload.
     *        doRead() skips publishing when the new payload matches this.
     */
    String      _lastPayload;

    void doRead();
    void publishPayload(const char* payload);
};