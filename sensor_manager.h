#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <MFRC522_I2C.h>
#include "sensor_base.h"
#include "ColorSensor.h"
#include "ble_emulator.h"

class SensorManager {
public:
    static void begin();
    static void loop();
    static void setManualSensorType(uint8_t index);
    
private:
    static bool readNfcCard();
};

#endif
