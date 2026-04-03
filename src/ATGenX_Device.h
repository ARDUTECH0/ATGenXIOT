/**
 * @file    ATGenX_Device.h
 * @brief   Relay device abstraction for the ATGenX IoT platform
 * @version 2.0.0
 *
 * Represents a single relay-controlled output (GPIO pin) that can be
 * commanded over MQTT, controlled locally, and queried for state.
 *
 * Topic convention
 * ─────────────────
 *   Command : atgenx/<userId>/<deviceId>/cmd
 *   State   : atgenx/<userId>/<deviceId>/state
 *
 * Accepted command payloads
 * ──────────────────────────
 *   Plain  : "1" | "0" | "on" | "off" | "true" | "false" | "toggle"
 *   JSON   : {"state":1} | {"state":"ON"} | {"state":"OFF"}
 */

#pragma once

#include <Arduino.h>

class ATGenX_Hub;   // Forward declaration

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Device
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Device {
public:

    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Construct a relay device.
     *
     * @param pin       GPIO pin connected to the relay.
     * @param deviceId  Unique identifier used in MQTT topics (e.g. "relay1").
     * @param activeLow Set true when the relay module triggers on LOW signal.
     *                  Default is true (most relay boards).
     */
    ATGenX_Device(uint8_t     pin,
                  const char* deviceId,
                  bool        activeLow = true);

    // ───────────────────────────────────────────────────────────────────────
    // Control API
    // ───────────────────────────────────────────────────────────────────────

    /** @brief Energise the relay (logical ON). */
    void turnOn();

    /** @brief De-energise the relay (logical OFF). */
    void turnOff();

    /** @brief Invert the current relay state. */
    void toggle();

    // ───────────────────────────────────────────────────────────────────────
    // Query API
    // ───────────────────────────────────────────────────────────────────────

    /** @brief Returns true when the relay is logically ON. */
    bool        isOn()       const;

    /** @brief Returns the GPIO pin number. */
    uint8_t     getPin()     const;

    /** @brief Returns the device identifier string. */
    const char* getId()      const;

    /**
     * @brief Returns the full MQTT path segment (userId/deviceId).
     *        Only valid after attachTo() has been called.
     */
    const char* getFullPath() const;

    // ───────────────────────────────────────────────────────────────────────
    // Callbacks
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Register a callback that fires whenever the relay state changes.
     *
     * @param cb  Function with signature: void fn(bool newState)
     *            Pass nullptr to clear.
     */
    void onStateChange(void (*cb)(bool newState));

    // ───────────────────────────────────────────────────────────────────────
    // Internal – called by ATGenX_Hub, not for end-user use
    // ───────────────────────────────────────────────────────────────────────

    /** @internal Binds this device to its parent hub and builds topic paths. */
    void attachTo(ATGenX_Hub* hub);

    /** @internal Parses and executes an incoming MQTT command payload. */
    void handleCommand(const char* payload, unsigned int len);

    /** @internal Publishes the current state to the state topic (retained). */
    void publishState() const;

private:

    // Hardware
    uint8_t _pin;
    bool    _activeLow;
    bool    _state;

    // Identity
    String  _id;
    String  _fullPath;
    String  _topicState;

    // Parent
    ATGenX_Hub* _hub;

    // User callback
    void (*_onStateChange)(bool);

    // ── Helpers ────────────────────────────────────────────────────────────

    /**
     * @brief Drives the GPIO and fires the callback.
     *        Single point of truth for all relay state changes.
     */
    void applyState(bool newState);

    /**
     * @brief Parses a plain-text or JSON payload into a tri-state result.
     * @return  1 = ON,  0 = OFF,  -1 = unrecognised / toggle
     */
    int8_t parseCommand(const char* payload) const;
};