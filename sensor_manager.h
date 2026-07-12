#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <MFRC522_I2C.h>
#include "sensor_base.h"
#include "VEML6040ColorSensor.h"
#include "ble_emulator.h"

class IdleSensor : public LegoSensor {
public:
    void begin() override {}
    uint16_t getProductId() override { return 0; }
    bool isConnected() override { return true; }
    void buildNotification(uint8_t* buffer, size_t& length) override {
        length = 0;
    }
};

class SensorManager {
public:
    static void begin();
    static void loop();
    static bool readNfcCard();
    static void detectSensors();
    
private:
};

#endif
