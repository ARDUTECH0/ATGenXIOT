/**
 * @file    ATGenX_Hub.h
 * @brief   Central hub controller for the ATGenX IoT platform
 * @version 2.1.0
 *
 * ATGenX_Hub manages:
 *  – Wi-Fi connectivity with automatic reconnection
 *  – MQTT session lifecycle (connect, subscribe, publish, keepalive)
 *  – Device registry  (up to MAX_DEVICES  relay/output devices)
 *  – Sensor registry  (up to MAX_SENSORS  read-only sensor devices)
 *  – Inbound command routing to the correct ATGenX_Device
 *
 * Quick start
 * ───────────
 * @code
 *   #include <ATGenX.h>
 *
 *   ATGenX_Hub          hub("user123");
 *   ATGenX_Device       relay1(5,  "relay1");
 *   ATGenX_Device       relay2(18, "relay2");
 *   ATGenX_DHT          dht(4,  "dht1");
 *   ATGenX_PIR          pir(13, "pir1");
 *
 *   void setup() {
 *       hub.onError([](ATGenX_Error e, const char* d) {
 *           Serial.printf("[Error %d] %s\n", (int)e, d);
 *       });
 *       hub.begin("MySSID", "MyPass", "mqttUser", "mqttPass");
 *       hub.attach(relay1);
 *       hub.attach(relay2);
 *       hub.attachSensor(dht);
 *       hub.attachSensor(pir);
 *       dht.begin();
 *       pir.begin();
 *   }
 *
 *   void loop() {
 *       hub.loop();   // drives relays, sensors, and MQTT — that's it
 *   }
 * @endcode
 */

#pragma once

#include <Arduino.h>

// Board-specific WiFi header
#if defined(ESP32)
#   include <WiFi.h>
#elif defined(ESP8266)
#   include <ESP8266WiFi.h>
#else
#   error "ATGenX supports ESP32 and ESP8266 only."
#endif

#include <PubSubClient.h>

class ATGenX_Device;    // Forward declaration
class ATGenX_Sensor;    // Forward declaration

// ═══════════════════════════════════════════════════════════════════════════
// Error codes
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Error conditions reported through the onError() callback.
 */
enum class ATGenX_Error : uint8_t {
    WIFI_TIMEOUT    = 1,  ///< Wi-Fi association timed out; board restarted.
    MQTT_FAILED     = 2,  ///< MQTT connection attempts exhausted.
    MQTT_RECONNECT  = 3,  ///< MQTT dropped; automatic reconnection started.
    DEVICE_LIMIT    = 4,  ///< attach() called after MAX_DEVICES reached.
    SENSOR_LIMIT    = 5,  ///< attachSensor() called after MAX_SENSORS reached.
    PUBLISH_FAILED  = 6,  ///< publish() returned false (queue full / disconnected).
};

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Hub
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Hub {
public:

    // ───────────────────────────────────────────────────────────────────────
    // Constants
    // ───────────────────────────────────────────────────────────────────────

    static constexpr size_t   MAX_DEVICES       = 16;
    static constexpr size_t   MAX_SENSORS       = 8;
    static constexpr uint8_t  WIFI_TIMEOUT_SEC  = 30;
    static constexpr uint8_t  MQTT_MAX_RETRIES  = 5;
    static constexpr uint16_t MQTT_RETRY_MS     = 3000;
    static constexpr uint16_t MQTT_KEEPALIVE_S  = 60;

    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Construct a hub for the given user account.
     * @param userId  Platform user ID — becomes the second topic segment.
     */
    explicit ATGenX_Hub(const char* userId);

    // ───────────────────────────────────────────────────────────────────────
    // Lifecycle
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Connect to Wi-Fi and the MQTT broker.
     *
     * Blocks until both connections succeed or the Wi-Fi timeout fires.
     * Call once in setup(), AFTER registering any callbacks.
     *
     * @param ssid          Wi-Fi network name.
     * @param password      Wi-Fi password.
     * @param mqttUsername  MQTT broker credentials.
     * @param mqttPassword  MQTT broker credentials.
     */
    void begin(const char* ssid,
               const char* password,
               const char* mqttUsername,
               const char* mqttPassword);

    /**
     * @brief Must be called every iteration of loop().
     *
     * Drives the MQTT client, triggers automatic reconnection if the session
     * has dropped, and calls loop() on every registered sensor.
     */
    void loop();

    // ───────────────────────────────────────────────────────────────────────
    // Device registry  (outputs / relays)
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Register an output device (relay) with this hub.
     *
     * Calls device.attachTo(this) internally, builds topic paths, and
     * subscribes to the device's command topic if already connected.
     *
     * @param device  Reference to an ATGenX_Device instance.
     */
    void attach(ATGenX_Device& device);

    /** @brief Number of output devices currently registered. */
    size_t deviceCount() const;

    /**
     * @brief Returns a pointer to the device at the given index, or nullptr.
     *        Index is zero-based; valid range is [0, deviceCount()).
     */
    const ATGenX_Device* getDevice(size_t index) const;

    // ───────────────────────────────────────────────────────────────────────
    // Sensor registry  (inputs / read-only)
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Register a sensor with this hub.
     *
     * Calls sensor.attachTo(this) internally.  The hub will call
     * sensor.loop() every iteration — no extra code needed in your sketch.
     *
     * @param sensor  Reference to any ATGenX_Sensor subclass instance.
     */
    void attachSensor(ATGenX_Sensor& sensor);

    /** @brief Number of sensors currently registered. */
    size_t sensorCount() const;

    /**
     * @brief Returns a pointer to the sensor at the given index, or nullptr.
     *        Index is zero-based; valid range is [0, sensorCount()).
     */
    const ATGenX_Sensor* getSensor(size_t index) const;

    // ───────────────────────────────────────────────────────────────────────
    // Status
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Returns true when the MQTT session is active.
     * @note  Not const — PubSubClient::connected() is not const.
     */
    bool isConnected();

    /** @brief Returns the user ID supplied at construction. */
    const char* getUserId() const;

    /** @brief Returns a human-readable board identifier string. */
    const char* getBoardType() const;

    // ───────────────────────────────────────────────────────────────────────
    // Callbacks
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Register an error handler.
     *
     * @param cb  Function: void fn(ATGenX_Error, const char* detail)
     *            Pass nullptr to clear.
     */
    void onError(void (*cb)(ATGenX_Error error, const char* detail));

    /**
     * @brief Register a connection-state change handler.
     *
     * @param cb  Function: void fn(bool connected)
     *            Pass nullptr to clear.
     */
    void onConnectionChange(void (*cb)(bool connected));

    // ───────────────────────────────────────────────────────────────────────
    // Internal publish/subscribe — used by ATGenX_Device and ATGenX_Sensor
    // ───────────────────────────────────────────────────────────────────────

    /** @internal Publish a payload to an arbitrary topic. */
    bool publish(const char* topic, const char* payload, bool retained = false);

    /** @internal Subscribe to a topic at QoS 1. */
    bool subscribe(const char* topic);

private:

    // ── MQTT broker config (compile-time defaults) ──────────────────────────
    static const char*    _brokerHost;
    static const uint16_t _brokerPort;

    // ── Singleton for static MQTT callback ──────────────────────────────────
    static ATGenX_Hub* _instance;

    // ── Identity ────────────────────────────────────────────────────────────
    String _userId;
    String _clientId;

    // ── Network ─────────────────────────────────────────────────────────────
    WiFiClient   _wifiClient;
    PubSubClient _mqtt;

    String _mqttUser;
    String _mqttPass;

    // ── Output device registry ───────────────────────────────────────────────
    ATGenX_Device* _devices[MAX_DEVICES];
    size_t         _deviceCount;

    // ── Sensor registry ──────────────────────────────────────────────────────
    ATGenX_Sensor* _sensors[MAX_SENSORS];
    size_t         _sensorCount;

    // ── User callbacks ───────────────────────────────────────────────────────
    void (*_onError)(ATGenX_Error, const char*);
    void (*_onConnectionChange)(bool);

    // ── Private helpers ──────────────────────────────────────────────────────

    /** Blocks until Wi-Fi is associated or timeout fires (then restarts). */
    void connectWiFi(const char* ssid, const char* pass);

    /** Attempts MQTT_MAX_RETRIES connections; invokes _onError on failure. */
    void connectMQTT();

    /** Resubscribes all devices and publishes their retained states. */
    void onMqttConnected();

    /** Routes an inbound message to the matching device. */
    void handleMessage(char* topic, byte* payload, unsigned int len);

    /** Reports an error through _onError (if set) and Serial. */
    void reportError(ATGenX_Error error, const char* detail) const;

    /** Generates a board-unique client ID. */
    String buildClientId() const;

    /** Static trampoline – PubSubClient requires a plain function pointer. */
    static void mqttCallback(char* topic, byte* payload, unsigned int len);
};