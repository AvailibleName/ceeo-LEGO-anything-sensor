#ifndef COLOR_SENSOR_H
#define COLOR_SENSOR_H

#include "sensor_base.h"

// Example Color Sensor class.
// In a real implementation, you would talk to a TCS34725 or similar over I2C here.
class ColorSensor : public LegoSensor {
private:
    uint8_t current_color = 0; // LEGO_COLOR_NONE initially
    
public:
    void begin() override {
        // Initialize real I2C sensor here
    }

    uint16_t getProductId() override {
        return 514; // PRODUCT_GROUP_DEVICE_COLOR_SENSOR
    }

    void buildNotification(uint8_t* buffer, size_t& length) override {
        // ColorSensorNotification Format:
        // Header (1 byte, ID = 12)
        // FormatString = "<bBHHHHBB"
        // (color, reflection, rawRed, rawGreen, rawBlue, hue, saturation, value)
        
        buffer[0] = 12; // COLOR_SENSOR_NOTIFICATION
        
        // Example mock data:
        int8_t color = 2; // Purple (just as a mock value)
        uint8_t reflection = 50;
        uint16_t rawRed = 100;
        uint16_t rawGreen = 150;
        uint16_t rawBlue = 200;
        uint16_t hue = 270;
        uint8_t saturation = 100;
        uint8_t value = 80;

        buffer[1] = color;
        buffer[2] = reflection;
        
        buffer[3] = rawRed & 0xFF;
        buffer[4] = rawRed >> 8;
        
        buffer[5] = rawGreen & 0xFF;
        buffer[6] = rawGreen >> 8;
        
        buffer[7] = rawBlue & 0xFF;
        buffer[8] = rawBlue >> 8;
        
        buffer[9] = hue & 0xFF;
        buffer[10] = hue >> 8;
        
        buffer[11] = saturation;
        buffer[12] = value;
        
        length = 13;
    }
};

#endif
