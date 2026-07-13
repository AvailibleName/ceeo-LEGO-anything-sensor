#ifndef BLE_EMULATOR_H
#define BLE_EMULATOR_H

#include <Arduino.h>
#include <bluefruit.h>
#include "sensor_base.h"

class BleEmulator {
public:
    static void begin();
    static void updateManufacturerData(uint16_t productGroupDevice, uint8_t appColor, uint16_t cardSerial);
    static void disconnect();
    static void startAdvertising();
    static void stopAdvertising();
    static void setSensor(LegoSensor* sensor);
    static void loop();

    // Callbacks
    static void connect_callback(uint16_t conn_handle);
    static void disconnect_callback(uint16_t conn_handle, uint8_t reason);
    static void rx_write_callback(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);
};

#endif
