#include "ATGenX_SoundSensor.h"

ATGenX_SoundSensor::ATGenX_SoundSensor(uint8_t     digitalPin,
                                       uint8_t     analogPin,
                                       const char* sensorId,
                                       SoundMode   mode,
                                       uint32_t    intervalMs,
                                       int         threshold)
    : ATGenX_Sensor(sensorId, intervalMs),
      _dPin(digitalPin),
      _aPin(analogPin),
      _mode(mode),
      _threshold(threshold),
      _lastAnalog(-9999),
      _lastDigital(-1)
{}

void ATGenX_SoundSensor::begin() {
    if (_dPin != 255) pinMode(_dPin, INPUT);
}

bool ATGenX_SoundSensor::readAndBuildPayload(char* buf, size_t sz) {

    if (_mode == SoundMode::DIGITAL_ONLY) {
        if (_dPin == 255) return false;
        const int state = digitalRead(_dPin);
        if (state == _lastDigital) return false;
        _lastDigital = state;
        snprintf(buf, sz, "{\"state\":%d}", state);
        return true;
    }

    if (_mode == SoundMode::ANALOG_ONLY) {
        if (_aPin == 255) return false;
        const int raw = analogRead(_aPin);

        // Debug مؤقت — شوف الـ raw value
        Serial.print(F("[Sound] raw="));
        Serial.print(raw);
        Serial.print(F("  last="));
        Serial.print(_lastAnalog);
        Serial.print(F("  diff="));
        Serial.println(raw - _lastAnalog);

        // أول قراءة — _lastAnalog = -9999 دايماً هيعدي
        const int diff = (raw > _lastAnalog)
                       ? (raw - _lastAnalog)
                       : (_lastAnalog - raw);

        if (_lastAnalog != -9999 && diff < _threshold) return false;

        _lastAnalog = raw;
        snprintf(buf, sz, "{\"value\":%d}", raw);
        return true;
    }

    // BOTH
    const int state = (_dPin != 255) ? digitalRead(_dPin) : -1;
    const int raw   = (_aPin != 255) ? analogRead(_aPin)  : -1;

    const int diff = (raw > _lastAnalog)
                   ? (raw - _lastAnalog)
                   : (_lastAnalog - raw);

    const bool stateChanged  = (state != -1) && (state != _lastDigital);
    const bool analogChanged = (raw   != -1) &&
                               (_lastAnalog == -9999 || diff >= _threshold);

    if (!stateChanged && !analogChanged) return false;

    if (stateChanged)  _lastDigital = state;
    if (analogChanged) _lastAnalog  = raw;

    if (state != -1 && raw != -1)
        snprintf(buf, sz, "{\"state\":%d,\"value\":%d}", state, raw);
    else if (state != -1)
        snprintf(buf, sz, "{\"state\":%d}", state);
    else
        snprintf(buf, sz, "{\"value\":%d}", raw);

    return true;
}