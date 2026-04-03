/**
 * @file    ATGenX_Discovery.h
 * @brief   MQTT Discovery announcer for the ATGenX IoT platform
 * @version 1.1.0
 *
 * Periodically publishes a retained JSON announcement so any subscriber
 * (mobile app, dashboard, other devices) can detect this hub automatically.
 *
 * Topic
 * ──────
 *   atgenx/<userId>/discovery
 *
 * Announcement payload (JSON, via ArduinoJson)
 * ──────────────────────────────────────────────
 * @code
 * {
 *   "userId"  : "user123",
 *   "board"   : "ESP32",
 *   "ip"      : "192.168.1.42",
 *   "rssi"    : -67,
 *   "uptime"  : 120,
 *   "status"  : "online",
 *   "devices" : [
 *     { "id": "relay1", "pin": 5,  "state": 1 },
 *     { "id": "relay2", "pin": 18, "state": 0 }
 *   ]
 * }
 * @endcode
 *
 * Usage
 * ──────
 * @code
 *   #include <ATGenX.h>
 *
 *   ATGenX_Hub       hub("user123");
 *   ATGenX_Device    relay1(5, "relay1");
 *   ATGenX_Discovery discovery(hub, 30000);   // announce every 30 s
 *
 *   void setup() {
 *       hub.begin(...);
 *       hub.attach(relay1);
 *       discovery.begin();           // call AFTER attach()
 *   }
 *
 *   void loop() {
 *       hub.loop();
 *       discovery.loop();
 *   }
 * @endcode
 *
 * Subscribe on MQTT Explorer / mobile app
 * ─────────────────────────────────────────
 *   atgenx/user123/discovery    — specific user
 *   atgenx/+/discovery          — all users (wildcard)
 */

#pragma once

#include <Arduino.h>
#include "ATGenX_Hub.h"
#include "ATGenX_Device.h"

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Discovery
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Discovery {
public:

    // ───────────────────────────────────────────────────────────────────────
    // Constants
    // ───────────────────────────────────────────────────────────────────────

    static constexpr uint32_t DEFAULT_INTERVAL_MS = 30000UL;   ///< 30 s
    static constexpr uint32_t MIN_INTERVAL_MS     =  5000UL;   ///<  5 s minimum

    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Attach discovery to an existing hub.
     *
     * @param hub         Reference to the owning ATGenX_Hub.
     * @param intervalMs  Announcement interval (ms). Clamped to MIN_INTERVAL_MS.
     */
    explicit ATGenX_Discovery(ATGenX_Hub& hub,
                              uint32_t    intervalMs = DEFAULT_INTERVAL_MS);

    // ───────────────────────────────────────────────────────────────────────
    // Lifecycle
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Publishes the first announcement immediately.
     *        Call once in setup(), AFTER hub.begin() and all hub.attach() calls.
     */
    void begin();

    /**
     * @brief Must be called every iteration of loop().
     *        Re-announces when the interval has elapsed and hub is connected.
     */
    void loop();

    // ───────────────────────────────────────────────────────────────────────
    // Control
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Force an immediate announcement regardless of the timer.
     *        Useful after a reconnection or a manual device state change.
     */
    void announce();

    /**
     * @brief Update the announcement interval at runtime.
     * @param intervalMs  New interval (ms); clamped to MIN_INTERVAL_MS.
     */
    void setInterval(uint32_t intervalMs);

    /** @brief Returns the MQTT topic this instance publishes to. */
    const char* getTopic() const;

private:

    ATGenX_Hub& _hub;
    uint32_t    _intervalMs;
    uint32_t    _lastAnnounceMs;
    String      _topic;

    /** Builds the ArduinoJson document and publishes it as a retained message. */
    void publishAnnouncement() const;
};