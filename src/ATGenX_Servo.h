/**
 * @file    ATGenX_Servo.h
 * @brief   Servo output for the ATGenX IoT platform
 * @version 2.2.0
 *
 * Not included by <ATGenXIOT.h> because it needs a servo library:
 *   ESP32   → "ESP32Servo" (Library Manager)
 *   ESP8266 → the built-in Servo library
 *
 * Commands  (atgenx/<clientId>/<deviceId>/cmd):
 *   {"angle":90}          move to 0…180°
 *   {"state":1} / {"state":0}   → 180° / 0°   (works with ON/OFF switches)
 *   {"cmd":"getState"}    report the current angle
 * State     (atgenx/<clientId>/<deviceId>/state, retained):
 *   {"angle":90,"state":1}
 *
 * @code
 *   #include <ATGenXIOT.h>
 *   #include <ATGenX_Servo.h>
 *
 *   ATGenX_Servo door(13, "door");
 *   …
 *   hub.attach(door);
 * @endcode
 */

#pragma once
#include "ATGenX_Device.h"
#include "ATGenX_Hub.h"

#if defined(ESP32)
#   include <ESP32Servo.h>
#else
#   include <Servo.h>
#endif

class ATGenX_Servo : public ATGenX_Device {
public:

    /**
     * @param pin          PWM-capable GPIO.
     * @param deviceId     Unique ID for the MQTT topic (e.g. "door").
     * @param startAngle   Angle applied on start-up.
     */
    ATGenX_Servo(uint8_t pin, const char* deviceId, uint8_t startAngle = 0)
        : ATGenX_Device(pin, deviceId, /*activeLow=*/false),
          _angle(startAngle > 180 ? 180 : startAngle)
    {}

    /** @brief Move to 0…180° and report it. */
    void write(int angle) {
        _angle = (uint8_t)constrain(angle, 0, 180);
        if (!_servo.attached()) _servo.attach(_pin);
        _servo.write(_angle);
        Serial.print(F("[ATGenX] '"));
        Serial.print(_id);
        Serial.print(F("' → "));
        Serial.print(_angle);
        Serial.println(F("°"));
        publishState();
    }

    int angle() const { return _angle; }

    void handleCommand(const char* payload, unsigned int len) override {
        constexpr size_t MAX_CMD = 255;
        if (len == 0 || len > MAX_CMD) return;
        char buf[MAX_CMD + 1];
        memcpy(buf, payload, len);
        buf[len] = '\0';

        if (const char* a = strstr(buf, "\"angle\"")) {
            a = strchr(a + 7, ':');
            if (a) { write(atoi(a + 1)); return; }
        }
        // ON/OFF style commands sweep fully (never digitalWrite a PWM pin)
        switch (parseCommand(buf)) {
            case  1: write(180); break;
            case  0: write(0);   break;
            case -2: write(_angle > 0 ? 0 : 180); break;
            case -3: publishState(); break;
            default:
                Serial.print(F("[ATGenX] '"));
                Serial.print(_id);
                Serial.print(F("' – unrecognised command: "));
                Serial.println(buf);
        }
    }

    /** @internal Also moves to the start angle once the hub is ready. */
    void begin() { write(_angle); }

    void publishState() const override {
        if (!_hub) return;
        char payload[64];
        snprintf(payload, sizeof(payload), "{\"angle\":%u,\"state\":%d}", _angle, _angle > 0 ? 1 : 0);
        _hub->publish(_topicState.c_str(), payload, /*retained=*/true);
    }

protected:
    void setupPin() override {}   // the Servo library owns this pin

private:
    Servo   _servo;
    uint8_t _angle;
};
