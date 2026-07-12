#ifndef OLED_SENSOR_H
#define OLED_SENSOR_H

#include "sensor_base.h"
#include <Wire.h>

class OledSensor : public LegoSensor {
public:
    OledSensor();
    ~OledSensor() override = default;

    void begin() override;
    uint16_t getProductId() override;
    bool isConnected() override;
    void buildNotification(uint8_t* buffer, size_t& length) override;
    void handleDataReceived(const uint8_t* payload, size_t length) override;

private:
    void sendCommand(uint8_t cmd);
    void sendData(const uint8_t* data, size_t len);
    void clearDisplay();
    void setCursor(uint8_t col, uint8_t page);
    void printChar(char c);
    
    uint8_t _i2cAddress;
    uint8_t _cursorCol;
    uint8_t _cursorPage;
};

#endif
