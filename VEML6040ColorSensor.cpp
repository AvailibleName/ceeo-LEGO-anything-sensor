#include "VEML6040ColorSensor.h"
#include <math.h>

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

VEML6040ColorSensor::VEML6040ColorSensor() {
}

void VEML6040ColorSensor::begin() {
    Wire.beginTransmission(VEML6040_I2C_ADDR);
    Wire.write(REG_CONF);
    // Config: Auto mode, 40ms integration time (0x00)
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission();
}

uint16_t VEML6040ColorSensor::getProductId() {
    return 514; // PRODUCT_GROUP_DEVICE_COLOR_SENSOR
}

bool VEML6040ColorSensor::isConnected() {
    Wire.beginTransmission(VEML6040_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

uint16_t VEML6040ColorSensor::readRegister16(uint8_t reg) {
    Wire.beginTransmission(VEML6040_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(VEML6040_I2C_ADDR, (uint8_t)2);
    
    if (Wire.available() >= 2) {
        uint8_t lsb = Wire.read();
        uint8_t msb = Wire.read();
        return (msb << 8) | lsb;
    }
    return 0;
}

void VEML6040ColorSensor::rgbToHsv(uint16_t r, uint16_t g, uint16_t b, uint16_t& h, uint8_t& s, uint8_t& v) {
    // Normalize to 0.0 - 1.0 (assuming 16-bit color maxes around 65535, 
    // but practically indoors it might max out lower, so we scale relative to the max of the three)
    float max_rgb = max(r, max(g, b));
    if (max_rgb == 0) {
        h = 0; s = 0; v = 0;
        return;
    }

    float rf = r / max_rgb;
    float gf = g / max_rgb;
    float bf = b / max_rgb;

    float cmax = max(rf, max(gf, bf));
    float cmin = min(rf, min(gf, bf));
    float delta = cmax - cmin;

    float h_float = 0;
    if (delta == 0) {
        h_float = 0;
    } else if (cmax == rf) {
        h_float = 60.0f * fmod(((gf - bf) / delta), 6.0f);
    } else if (cmax == gf) {
        h_float = 60.0f * (((bf - rf) / delta) + 2.0f);
    } else if (cmax == bf) {
        h_float = 60.0f * (((rf - gf) / delta) + 4.0f);
    }

    if (h_float < 0) h_float += 360.0f;

    float s_float = (cmax == 0) ? 0 : (delta / cmax);
    
    // Scale value based on max_rgb relative to a reasonable indoor brightness (e.g., 10000)
    // Cap at 1.0
    float v_float = max_rgb / 10000.0f;
    if (v_float > 1.0f) v_float = 1.0f;

    h = (uint16_t)h_float;
    s = (uint8_t)(s_float * 100.0f);
    v = (uint8_t)(v_float * 100.0f);
}

int8_t VEML6040ColorSensor::matchLegoColor(uint16_t h, uint8_t s, uint8_t v) {
    // LEGO Firmware Colors:
    // -1 = None, 0 = Black, 1 = Magenta, 2 = Purple, 3 = Blue, 4 = Azure,
    // 5 = Turquoise, 6 = Green, 7 = Yellow, 8 = Orange, 9 = Red, 10 = White
    
    // Ambient light significantly desaturates reflective colors without a dedicated LED.
    // We must relax the Saturation and Value thresholds.
    if (v < 3) return 0; // Black
    if (s < 10 && v > 70) return 10; // White
    if (s < 5) return -1; // None (Gray)

    if (h < 20 || h >= 345) return 9; // Red
    if (h >= 20 && h < 50) return 8; // Orange
    if (h >= 50 && h < 85) return 7; // Yellow
    if (h >= 85 && h < 150) return 6; // Green
    if (h >= 150 && h < 180) return 5; // Turquoise
    if (h >= 180 && h < 210) return 4; // Azure
    if (h >= 210 && h < 260) return 3; // Blue
    if (h >= 260 && h < 290) return 2; // Purple
    if (h >= 290 && h < 345) return 1; // Magenta

    return -1; // Fallback
}

void VEML6040ColorSensor::buildNotification(uint8_t* buffer, size_t& length) {
    // ColorSensorNotification Format:
    // Header (1 byte, ID = 12)
    // FormatString = "<bBHHHHBB"
    // (color, reflection, rawRed, rawGreen, rawBlue, hue, saturation, value)
    
    uint16_t rawRed = readRegister16(REG_R_DATA);
    uint16_t rawGreen = readRegister16(REG_G_DATA);
    uint16_t rawBlue = readRegister16(REG_B_DATA);
    
    // Apply a simple white-balance calibration based on testing.
    // The user found that Red * 1.065 and Blue * 2.7 perfectly aligns the channels!
    uint32_t calibratedRed = (uint32_t)(rawRed * 1.065f);
    if (calibratedRed > 65535) calibratedRed = 65535;
    rawRed = (uint16_t)calibratedRed;

    uint32_t calibratedBlue = (uint32_t)(rawBlue * 2.7f);
    if (calibratedBlue > 65535) calibratedBlue = 65535;
    rawBlue = (uint16_t)calibratedBlue;
    
    uint16_t hue = 0;
    uint8_t saturation = 0;
    uint8_t value = 0;
    
    rgbToHsv(rawRed, rawGreen, rawBlue, hue, saturation, value);
    
    int8_t color = matchLegoColor(hue, saturation, value); 
    uint8_t reflection = value; // Use 'value' as a proxy for reflection

    buffer[0] = 12; // COLOR_SENSOR_NOTIFICATION
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
