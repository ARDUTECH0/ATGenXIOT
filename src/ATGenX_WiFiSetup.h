/**
 * @file    ATGenX_WiFiSetup.h
 * @brief   Put the board on the home Wi-Fi from a phone — no network name or password in the sketch.
 * @version 2.6.0
 *
 * A board with no saved network (or one it can't reach for a minute, or whose BOOT button was held 5 s)
 * opens its own temporary network "ATGenX-XXXX" (password "atgenx-setup") and answers at 192.168.4.1:
 *
 *   GET  /info  → {"device":"ATGenX-4F2A","board":"ESP32","fw":"2.6.0"}
 *   GET  /scan  → [{"ssid":"Home","rssi":-55,"secure":true}, …]
 *   POST /wifi  {"ssid":"Home","password":"…"} → {"ok":true}   saved on the board, which restarts and joins
 *   GET  /      a small page for any phone (no app): pick the network, type the password
 *
 * The ATGenX app does all of it by itself; the board's light blinks while it waits.
 * Used by ATGenX_Hub::begin(mqttUser, mqttPass) — the sketch needs nothing else.
 */
#pragma once

#include <Arduino.h>

#ifndef ATGENX_SETUP_PASSWORD
#   define ATGENX_SETUP_PASSWORD "atgenx-setup"
#endif
/** The light that blinks in setup mode (-1: none). */
#ifndef ATGENX_SETUP_LED
#   ifdef LED_BUILTIN
#       define ATGENX_SETUP_LED LED_BUILTIN
#   else
#       define ATGENX_SETUP_LED -1
#   endif
#endif
/** Held 5 s at any time: forget the network and go back to setup mode (-1: off). BOOT on most boards. */
#ifndef ATGENX_SETUP_BUTTON
#   define ATGENX_SETUP_BUTTON 0
#endif

class ATGenX_WiFiSetup {
public:
    /** The saved home network, if any. */
    static bool load(String& ssid, String& pass);
    static void save(const String& ssid, const String& pass);
    static void clear();

    /** "ATGenX-4F2A" — the last 4 hex digits of this board's MAC. */
    static String apName();

    /**
     * Setup mode: the temporary network and its page, until a network is sent (then it restarts).
     * With `retryAfterMs` > 0 and nobody connected, it restarts after that long to try the saved network again
     * (a router that was only off for a while). Never returns.
     */
    [[noreturn]] static void run(const char* boardType, uint32_t retryAfterMs = 0);

    /** Call often from loop(): BOOT held 5 s → forget the network and restart into setup mode. */
    static void watchButton();
};
