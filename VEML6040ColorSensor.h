#pragma once

#include "sensor_base.h"
#include <Arduino.h>
#include <Wire.h>

class VEML6040ColorSensor : public LegoSensor {
public:
    VEML6040ColorSensor();
    virtual ~VEML6040ColorSensor() = default;

    void begin() override;
    uint16_t getProductId() override;
    void buildNotification(uint8_t* buffer, size_t& length) override;
    bool isConnected() override;

private:
    static const uint8_t VEML6040_I2C_ADDR = 0x10;
    
    // VEML6040 Register Addresses
    static const uint8_t REG_CONF = 0x00;
    static const uint8_t REG_R_DATA = 0x08;
    static const uint8_t REG_G_DATA = 0x09;
    static const uint8_t REG_B_DATA = 0x0A;
    static const uint8_t REG_W_DATA = 0x0B;

    uint16_t readRegister16(uint8_t reg);
    
    // Math helpers for generating HSV and LEGO Color matches
    void rgbToHsv(uint16_t r, uint16_t g, uint16_t b, uint16_t& h, uint8_t& s, uint8_t& v);
    int8_t matchLegoColor(uint16_t h, uint8_t s, uint8_t v);
};
