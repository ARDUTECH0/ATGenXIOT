/**
 * @file    ATGenX_Vars.h
 * @brief   Your own variables: values on the dashboard that live in your code, not on a pin
 * @version 2.6.1
 *
 * Make a variable on the site (device page → "Your own variables"), copy its ID, and use it here. The dashboard
 * and your code then share it both ways: the dashboard changes it → your callback runs; your code changes it →
 * the dashboard shows it.
 *
 *   ATGenX_Switch  — on / off              dashboard: a switch, or a lamp that shows it
 *   ATGenX_Number  — a number              dashboard: a slider you move, or a value / gauge / chart you read
 *   ATGenX_Color   — an RGB colour         dashboard: a colour picker
 *
 * Topics (like every ATGenX device; the ID is the one the site gave the variable):
 *   atgenx/<clientId>/<ID>/cmd     {"state":1} | {"value":42.5} | {"color":"#FF8800"}      (from the dashboard)
 *   atgenx/<clientId>/<ID>/state   the same, as the board has it now (retained)
 *
 * @code
 *   #include <ATGenXIOT.h>
 *
 *   ATGenX_Hub    hub("atg_dev_xxxxxx");
 *   ATGenX_Switch party("home_party");      // the IDs from the site
 *   ATGenX_Number speed("home_fan_speed");
 *   ATGenX_Color  strip("home_strip");
 *
 *   void setup() {
 *       hub.attach(party);
 *       hub.attach(speed);
 *       hub.attach(strip);
 *       party.onChange([](bool on) { Serial.println(on ? "party!" : "quiet"); });
 *       strip.onChange([](uint8_t r, uint8_t g, uint8_t b) { analogWrite(25, r); analogWrite(26, g); analogWrite(27, b); });
 *       hub.begin("atg_usr_xxxx", "atg_sk_xxxx");
 *   }
 *
 *   void loop() {
 *       hub.loop();
 *       if (party.isOn()) speed.set(analogRead(34) / 40.95);   // your reading, on the dashboard
 *   }
 * @endcode
 *
 * A number you set often is sent at most once per second (the latest value always arrives).
 */

#pragma once

#include <Arduino.h>
#include "ATGenX_Device.h"
#include "ATGenX_Hub.h"

namespace atgenx_vars {
    /** Where the value of "key" starts in a small flat JSON object, or nullptr. */
    inline const char* find(const char* json, const char* key) {
        char pattern[20];
        snprintf(pattern, sizeof(pattern), "\"%s\"", key);
        const char* p = strstr(json, pattern);
        if (!p) return nullptr;
        p = strchr(p + strlen(pattern), ':');
        if (!p) return nullptr;
        ++p;
        while (*p == ' ' || *p == '\t') ++p;
        return p;
    }

    /** The command as a C string (they are short), or false when it's empty or too long. */
    inline bool copy(const char* payload, unsigned int len, char* buf, size_t size) {
        if (len == 0 || len >= size) return false;
        memcpy(buf, payload, len);
        buf[len] = '\0';
        return true;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Switch — on / off
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Switch : public ATGenX_Device {
public:
    /** @param id  The variable's ID from the site. */
    explicit ATGenX_Switch(const char* id) : ATGenX_Device(id) {}

    /**
     * Turn it on or off from your code (the dashboard follows). Also: turnOn(), turnOff(), toggle(), isOn().
     * Only a change is sent, so calling it on every loop() is fine (the state is sent on every connect anyway).
     */
    void set(bool on) {
        if (on == _state) return;
        applyState(on);
    }

    /** Runs when the dashboard (or your code) changes it. */
    void onChange(void (*cb)(bool on)) { onStateChange(cb); }

    void publishState() const override {
        if (!_hub) return;
        char payload[96];
        snprintf(payload, sizeof(payload), "{\"state\":%d,\"label\":\"%s\",\"id\":\"%s\"}", _state ? 1 : 0, _state ? "ON" : "OFF", _id.c_str());
        _hub->publish(_topicState.c_str(), payload, /*retained=*/true);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Number — a number, both ways
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Number : public ATGenX_Device {
public:
    /**
     * @param id     The variable's ID from the site.
     * @param start  Its value until the dashboard or your code changes it.
     */
    explicit ATGenX_Number(const char* id, float start = 0) : ATGenX_Device(id), _value(start), _sent(start) {}

    float get() const { return _value; }
    operator float() const { return _value; }

    /** Change it from your code — a reading, a count, a level; the dashboard shows it. */
    void set(float v) {
        _value = v;
        _pending = true;
        tick();
    }

    /** Runs when the dashboard changes it (a slider moved). */
    void onChange(void (*cb)(float value)) { _onChange = cb; }

    /** @internal From the dashboard: {"value":42.5} (or {"cmd":"getState"}). */
    void handleCommand(const char* payload, unsigned int len) override {
        char buf[256];
        if (!atgenx_vars::copy(payload, len, buf, sizeof(buf))) return;
        if (const char* v = atgenx_vars::find(buf, "value")) {
            _value = atof(*v == '"' ? v + 1 : v);
            if (_onChange) _onChange(_value);
            _pending = false;
            publishState();
            return;
        }
        if (parseCommand(buf) == -3) publishState();
    }

    void publishState() const override {
        if (!_hub) return;
        String payload = String("{\"value\":") + String(_value, 3) + ",\"id\":\"" + _id + "\"}";
        _hub->publish(_topicState.c_str(), payload.c_str(), /*retained=*/true);
        _sentAt = millis();
        _sent = _value;
    }

    /** @internal Called by hub.loop(): sends the latest value once a second has passed since the last one. */
    void tick() override {
        if (!_pending || !_hub || !_hub->isConnected()) return;
        if (_sentAt && millis() - _sentAt < 1000) return;
        _pending = false;
        if (_value != _sent || !_sentAt) publishState();
    }

private:
    float _value;
    mutable float _sent;
    mutable uint32_t _sentAt = 0;
    bool _pending = false;
    void (*_onChange)(float) = nullptr;
};

// ═══════════════════════════════════════════════════════════════════════════
// ATGenX_Color — an RGB colour
// ═══════════════════════════════════════════════════════════════════════════

class ATGenX_Color : public ATGenX_Device {
public:
    /**
     * @param id     The variable's ID from the site.
     * @param start  0xRRGGBB until the dashboard or your code changes it (white by default).
     */
    explicit ATGenX_Color(const char* id, uint32_t start = 0xFFFFFF) : ATGenX_Device(id), _rgb(start & 0xFFFFFF) {}

    uint8_t  red()   const { return (_rgb >> 16) & 0xFF; }
    uint8_t  green() const { return (_rgb >> 8) & 0xFF; }
    uint8_t  blue()  const { return _rgb & 0xFF; }
    /** 0xRRGGBB */
    uint32_t rgb()   const { return _rgb; }

    void set(uint8_t r, uint8_t g, uint8_t b) { set(((uint32_t)r << 16) | ((uint32_t)g << 8) | b); }
    /** Only a change is sent, so calling it on every loop() is fine. */
    void set(uint32_t rgb) {
        if ((rgb & 0xFFFFFF) == _rgb) return;
        _rgb = rgb & 0xFFFFFF;
        publishState();
    }

    /** Runs when the dashboard picks a colour. */
    void onChange(void (*cb)(uint8_t r, uint8_t g, uint8_t b)) { _onChange = cb; }

    /** @internal From the dashboard: {"color":"#FF8800"} or {"value":16746496}. */
    void handleCommand(const char* payload, unsigned int len) override {
        char buf[256];
        if (!atgenx_vars::copy(payload, len, buf, sizeof(buf))) return;
        const char* c = atgenx_vars::find(buf, "color");
        if (c && c[0] == '"' && c[1] == '#') {
            _rgb = strtoul(c + 2, nullptr, 16) & 0xFFFFFF;
        } else if (const char* v = atgenx_vars::find(buf, "value")) {
            _rgb = strtoul(v, nullptr, 10) & 0xFFFFFF;
        } else {
            if (parseCommand(buf) == -3) publishState();
            return;
        }
        if (_onChange) _onChange(red(), green(), blue());
        publishState();
    }

    void publishState() const override {
        if (!_hub) return;
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"value\":%lu,\"color\":\"#%06lX\",\"id\":\"%s\"}", (unsigned long)_rgb, (unsigned long)_rgb, _id.c_str());
        _hub->publish(_topicState.c_str(), payload, /*retained=*/true);
    }

private:
    uint32_t _rgb;
    void (*_onChange)(uint8_t, uint8_t, uint8_t) = nullptr;
};
