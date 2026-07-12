#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H

#include <Arduino.h>

class LegoSensor {
public:
    virtual ~LegoSensor() = default;

    // Called once during setup when this sensor is selected.
    // Initialize your I2C/SPI/UART pins and sensor configuration here.
    virtual void begin() = 0;

    // Return the specific ProductGroupDevice ID for this sensor type.
    // Examples: 512 = Single Motor, 514 = Color Sensor
    virtual uint16_t getProductId() = 0;

    // Check if the sensor is physically connected
    virtual bool isConnected() { return true; }

    // Build the Notification Payload for the BLE TX characteristic.
    // Pack your sensor data into the 'buffer' according to the LEGO RPC format.
    // Update 'length' with the total number of bytes written.
    virtual void buildNotification(uint8_t* buffer, size_t& length) = 0;

    // Optional: Handle incoming data from the Host PC (e.g. text for an OLED)
    // The default implementation is empty so read-only sensors don't have to implement it.
    virtual void handleDataReceived(const uint8_t* payload, size_t length) {}
};

#endif
