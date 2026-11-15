/**
 * @file    ATGenX_Hub.h
 * @brief   Central hub controller for the ATGenX IoT platform
 * @version 2.3.0
 *
 * ATGenX_Hub manages:
 *  – Wi-Fi connectivity (the radio reconnects on its own)
 *  – MQTT session lifecycle with a NON-BLOCKING reconnect and back-off,
 *    so outputs and sensors keep running while the network is down
 *  – an online/offline status topic (MQTT last will)
 *  – Device registry  (up to MAX_DEVICES  relay/output devices)
 *  – Sensor registry  (up to MAX_SENSORS  read-only sensor devices)
 *  – Inbound command routing to the correct device
 *
 * Topics (clientId = the "Client ID" from your ATGENX account page):
 *   atgenx/<clientId>/<deviceId>/cmd       ← commands  {"state":1} / {"cmd":"getState"}
 *   atgenx/<clientId>/<deviceId>/state     → output state (retained)
 *   atgenx/<clientId>/<sensorId>/reading   → sensor readings
 *   atgenx/<clientId>/<boardId>/status     → {"online":true|false} (retained, last will)
 *   atgenx/<clientId>/discovery            → board announcement (ATGenX_Discovery)
 *   atgenx/<clientId>/<boardId>/ota        ← over-the-air update {"url","md5","size","version"}
 *
 * Quick start
 * ───────────
 * @code
 *   #include <ATGenXIOT.h>
 *
 *   ATGenX_Hub    hub("atg_dev_xxxxxx");   // Client ID
 *   ATGenX_Device lamp(2, "lamp");
 *   ATGenX_DHT    room(4, "room");
 *
 *   void setup() {
 *       Serial.begin(115200);
 *       hub.begin("MySSID", "MyPass", "atg_usr_…", "atg_sk_…");
 *       hub.attach(lamp);
 *       hub.attachSensor(room);
 *       room.begin();
 *   }
 *
 *   void loop() {
 *       hub.loop();   // drives outputs, sensors and MQTT — that's it
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
#if defined(ESP32)
#   include <WiFiClientSecure.h>
#elif defined(ESP8266)
#   include <WiFiClientSecureBearSSL.h>
#endif

// Broker defaults — override with hub.setServer() or by defining these
// before including the library.
#ifndef ATGENX_BROKER_HOST
#   define ATGENX_BROKER_HOST "device.atgenx.com"
#endif
#ifndef ATGENX_BROKER_PORT
#   define ATGENX_BROKER_PORT 6523
#endif

// Set to 1 for per-reading Serial logs (noisy).
#ifndef ATGENX_DEBUG
#   define ATGENX_DEBUG 0
#endif

/** Firmware version stamped by the ATGENX cloud build (0 = built elsewhere). */
extern uint32_t g_atgxFirmwareVersion;
struct ATGenX_FwVersion {
    explicit ATGenX_FwVersion(uint32_t v) { g_atgxFirmwareVersion = v; }
};
/** ATGENX project id stamped by the build ("" = unknown). */
extern const char* g_atgxProjectId;
struct ATGenX_ProjectId {
    explicit ATGenX_ProjectId(const char* id) { g_atgxProjectId = id; }
};

class ATGenX_Device;    // Forward declaration
class ATGenX_Sensor;    // Forward declaration

// ═══════════════════════════════════════════════════════════════════════════
// Error codes
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Error conditions reported through the onError() callback.
 */
enum class ATGenX_Error : uint8_t {
    WIFI_TIMEOUT    = 1,  ///< Wi-Fi association timed out in begin(); board restarted.
    MQTT_FAILED     = 2,  ///< MQTT login refused or broker unreachable (see detail).
    MQTT_RECONNECT  = 3,  ///< MQTT dropped; automatic reconnection started.
    DEVICE_LIMIT    = 4,  ///< attach() called after MAX_DEVICES reached.
    SENSOR_LIMIT    = 5,  ///< attachSensor() called after MAX_SENSORS reached.
    PUBLISH_FAILED  = 6,  ///< publish() returned false (disconnected / payload too large).
};

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Hub
// ═══════════════════════════════════════════════════════════════════════════

/// setLink() is here (2.9): a sketch can test for it and still build with an older library
#define ATGENX_HUB_HAS_LINK 1

class ATGenX_Hub {
public:

    // ───────────────────────────────────────────────────────────────────────
    // Constants
    // ───────────────────────────────────────────────────────────────────────

    static constexpr size_t   MAX_DEVICES       = 24;
    static constexpr size_t   MAX_SENSORS       = 24;
    static constexpr uint8_t  WIFI_TIMEOUT_SEC  = 30;
    static constexpr uint8_t  MQTT_BEGIN_TRIES  = 3;      ///< blocking tries inside begin()
    static constexpr uint32_t RETRY_MIN_MS      = 2000;   ///< first reconnect delay
    static constexpr uint32_t RETRY_MAX_MS      = 60000;  ///< back-off ceiling
    static constexpr uint16_t MQTT_KEEPALIVE_S  = 30;
    static constexpr uint16_t MQTT_BUFFER_BYTES = 1024;   ///< fits discovery + large readings

    // ───────────────────────────────────────────────────────────────────────
    // Construction
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Construct a hub for the given account.
     * @param userId  Account Client ID (atg_dev_…) — the second topic segment.
     */
    explicit ATGenX_Hub(const char* userId);

    // ───────────────────────────────────────────────────────────────────────
    // Lifecycle
    // ───────────────────────────────────────────────────────────────────────

    /**
     * @brief Use another broker (e.g. a local one while developing).
     *        Call before begin().
     */
    void setServer(const char* host, uint16_t port);

    /**
     * @brief Encrypt the MQTT connection (TLS, usually port 8883). Call before begin().
     *        The broker's certificate is checked against `rootCa` (PEM) — by default the
     *        Let's Encrypt root (ISRG Root X1) that ATGENX Cloud uses. ESP8266 boards also
     *        fetch the time (NTP) first, which certificate checks need.
     */
    void useTls(const char* rootCa = nullptr);

    /**
     * @brief TLS without checking the certificate — only for a test broker with a
     *        self-signed certificate. The link is encrypted but anyone could pose as the broker.
     */
    void useTlsInsecure();

    /**
     * @brief Connect to Wi-Fi and the MQTT broker.
     *
     * Waits for Wi-Fi (restarts the board after WIFI_TIMEOUT_SEC), then makes
     * a few quick MQTT attempts. If the broker is still unreachable, loop()
     * keeps retrying in the background.
     */
    void begin(const char* ssid,
               const char* password,
               const char* mqttUsername,
               const char* mqttPassword);

    /**
     * @brief Connect without a network name or password in the sketch (v2.6): the home Wi-Fi is set from the
     *        ATGenX app (Add a device) and kept on the board. With none saved — or when the saved one can't be
     *        reached for a minute — the board opens its setup network "ATGenX-XXXX". Holding BOOT 5 s at any
     *        time forgets the network. See ATGenX_WiFiSetup.h.
     */
    void begin(const char* mqttUsername, const char* mqttPassword);

    /**
     * @brief Connect through a network the sketch brings up itself (v2.8) — a SIM modem with TinyGSM
     *        (`TinyGsmClient`), Ethernet… instead of Wi-Fi. The broker, the account and everything after
     *        are the same. `linkUp` (optional) says whether that network is up: while it isn't, MQTT isn't
     *        retried and the sketch reconnects it (e.g. `modem.isGprsConnected()` / `gprsConnect()`).
     *        `via` names the link in the board's status ("cellular"). Updates over the air stay off on this
     *        link (the firmware download goes over Wi-Fi only).
     */
    void beginWithClient(Client& network,
                         const char* mqttUsername,
                         const char* mqttPassword,
                         bool (*linkUp)() = nullptr,
                         const char* via = "cellular");

    /**
     * @brief The sketch's own network, for the board's status (v2.9): its signal in dBm (0 = unknown), its
     *        kind ("4G", "2G", "LTE-M"…) and the operator's name — what "My boards" shows for a SIM board.
     *        Call it every minute or so (each value is an AT command to the modem). The status goes out again
     *        when the operator or the kind change, or the signal moves by 6 dB or more (then once a minute at
     *        most). Quotes and control characters are dropped from the words.
     */
    void setLink(int dbm, const char* kind, const char* carrier);

    /** @brief The signal the status carries: Wi-Fi's RSSI, or the one setLink() gave (0 when unknown). */
    int rssi() const;

    /**
     * @brief Must be called every iteration of loop().
     *
     * Drives the MQTT client, reconnects without blocking when the session
     * drops, and polls every registered sensor.
     */
    void loop();

    // ───────────────────────────────────────────────────────────────────────
    // Device registry  (outputs / relays)
    // ───────────────────────────────────────────────────────────────────────

    /** @brief Register an output device (relay, LED …). */
    void attach(ATGenX_Device& device);

    /** @brief Number of output devices currently registered. */
    size_t deviceCount() const;

    /** @brief Device at index [0, deviceCount()), or nullptr. */
    const ATGenX_Device* getDevice(size_t index) const;

    // ───────────────────────────────────────────────────────────────────────
    // Sensor registry  (inputs / read-only)
    // ───────────────────────────────────────────────────────────────────────

    /** @brief Register a sensor; the hub polls it from loop(). */
    void attachSensor(ATGenX_Sensor& sensor);

    /** @brief Number of sensors currently registered. */
    size_t sensorCount() const;

    /** @brief Sensor at index [0, sensorCount()), or nullptr. */
    const ATGenX_Sensor* getSensor(size_t index) const;

    // ───────────────────────────────────────────────────────────────────────
    // Status
    // ───────────────────────────────────────────────────────────────────────

    /** @brief True while the MQTT session is up. */
    bool isConnected();

    /** @brief Account Client ID supplied at construction. */
    const char* getUserId() const;

    /** @brief This board's MQTT client id (atgx-<chip id>). */
    const char* getClientId() const;

    /** @brief Human-readable board identifier. */
    const char* getBoardType() const;

    // ───────────────────────────────────────────────────────────────────────
    // Callbacks
    // ───────────────────────────────────────────────────────────────────────

    /** @brief void fn(ATGenX_Error, const char* detail); nullptr clears. */
    void onError(void (*cb)(ATGenX_Error error, const char* detail));

    /** @brief void fn(bool connected); nullptr clears. */
    void onConnectionChange(void (*cb)(bool connected));

    /**
     * @brief Over-the-air updates from the ATGENX site (on by default).
     *        The board downloads the new firmware, checks its MD5, flashes
     *        it and restarts — no USB cable needed.
     */
    void enableOta(bool on) { _otaEnabled = on; }

    /** @brief Firmware version from the ATGENX build (0 if built elsewhere). */
    uint32_t firmwareVersion() const { return g_atgxFirmwareVersion; }

    // ───────────────────────────────────────────────────────────────────────
    // Publish / subscribe — used by devices and sensors (and custom code)
    // ───────────────────────────────────────────────────────────────────────

    /** Publish a payload to an arbitrary topic. */
    bool publish(const char* topic, const char* payload, bool retained = false);

    /** Subscribe to a topic at QoS 1. */
    bool subscribe(const char* topic);

private:

    // ── Singleton for the static MQTT callback ──────────────────────────────
    static ATGenX_Hub* _instance;

    // ── Identity ────────────────────────────────────────────────────────────
    String _userId;
    String _clientId;
    String _statusTopic;
    String _otaTopic;
    bool   _otaEnabled = true;

    // ── Network ─────────────────────────────────────────────────────────────
    WiFiClient   _wifiClient;
#if defined(ESP32)
    WiFiClientSecure _tlsClient;
#elif defined(ESP8266)
    BearSSL::WiFiClientSecure _tlsClient;
    BearSSL::X509List*        _tlsAnchors = nullptr;
#endif
    /// the network came from the phone (begin(user, pass)): setup mode instead of restarting on a timeout
    bool        _phoneSetup = false;
    /// beginWithClient(): a network the sketch runs (a SIM modem), not this board's Wi-Fi
    bool        _external = false;
    bool      (*_linkUp)() = nullptr;
    const char* _via = "wifi";
    /// setLink(): the external network as the status shows it, and what the last status sent
    int      _linkDbm = 0;
    int      _sentDbm = 0;
    char     _linkKind[12] = "";
    char     _linkCarrier[40] = "";
    uint32_t _statusAt = 0;
    void     publishStatus();
    String      _savedSsid;
    String      _savedPass;
    bool        _tls = false;
    bool        _tlsInsecure = false;
    const char* _tlsCa = nullptr;
    PubSubClient _mqtt;

    String   _host;
    uint16_t _port;
    String   _mqttUser;
    String   _mqttPass;

    // ── Reconnect state ─────────────────────────────────────────────────────
    bool     _wasConnected;
    uint32_t _lastAttemptMs;
    uint32_t _retryMs;

    // ── Output device registry ──────────────────────────────────────────────
    ATGenX_Device* _devices[MAX_DEVICES];
    size_t         _deviceCount;

    // ── Sensor registry ─────────────────────────────────────────────────────
    ATGenX_Sensor* _sensors[MAX_SENSORS];
    size_t         _sensorCount;

    // ── User callbacks ──────────────────────────────────────────────────────
    void (*_onError)(ATGenX_Error, const char*);
    void (*_onConnectionChange)(bool);

    // ── Private helpers ─────────────────────────────────────────────────────
    void connectWiFi(const char* ssid, const char* pass);
    bool tryConnectMQTT();
    void serviceConnection();
    void onMqttConnected();
    void handleMessage(char* topic, byte* payload, unsigned int len);
    void handleOta(const char* payload, unsigned int len);
    void otaStatus(const char* state, long version, const char* error = nullptr);
    void reportError(ATGenX_Error error, const char* detail) const;
    String buildClientId() const;
    static const char* mqttStateText(int state);
    static void mqttCallback(char* topic, byte* payload, unsigned int len);
};
