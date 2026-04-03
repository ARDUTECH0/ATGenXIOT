#pragma once
#include "ATGenX_Sensor.h"

class ATGenX_SoundSensor : public ATGenX_Sensor {
public:

    enum class SoundMode : uint8_t {
        DIGITAL_ONLY,   ///< DO pin فقط
        ANALOG_ONLY,    ///< AO pin فقط
        BOTH            ///< DO + AO
    };

    ATGenX_SoundSensor(uint8_t     digitalPin,
                       uint8_t     analogPin,
                       const char* sensorId,
                       SoundMode   mode       = SoundMode::DIGITAL_ONLY,
                       uint32_t    intervalMs = 100,
                       int         threshold  = 20);

    void begin();

protected:
    bool readAndBuildPayload(char* buf, size_t sz) override;

private:
    uint8_t   _dPin;
    uint8_t   _aPin;
    SoundMode _mode;
    int       _threshold;
    int       _lastAnalog;
    int       _lastDigital;
};