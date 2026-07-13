#include "debug.h"
#include <Arduino.h>
#include <Wire.h>
#include "power_ui.h"
#include "sensor_manager.h"
#include "ble_emulator.h"

// Define Hardware Pins for Seeed XIAO nRF52840
#define BTN_PIN D0
#define LED_R D3
#define LED_G D2
#define LED_B D1

void setup() {
    // CRITICAL for battery operation: Allow time for external sensors (PN532, VEML6040)
    // to physically power up and stabilize before we attempt to talk to them over I2C.
    // Without this, I2C devices can lock the bus if they are polled while still booting.
    delay(2000);

    Serial.begin(115200);
    
    DEBUG_PRINTLN("LEGO Smart Sensor Emulator starting...");

    // Initialize Wire for I2C (default SDA=D4, SCL=D5 on XIAO)
    Wire.begin();

    // Initialize UI and Power Management
    PowerUI::begin(BTN_PIN, LED_R, LED_G, LED_B);

    // Initialize Sensor Manager and PN532
    SensorManager::begin();

    // Initialize BLE Emulator
    BleEmulator::begin();

    // Wire up UI callbacks

    PowerUI::setPairingToggledCallback([](bool pairingOn) {
        if (pairingOn) {
            BleEmulator::startAdvertising();
        } else {
            BleEmulator::stopAdvertising();
        }
    });

    DEBUG_PRINTLN("System Ready.");
}

void loop() {
    // 1. Process Button and UI State Machine
    PowerUI::loop();
    
    UIState state = PowerUI::getState();

    // 2. Run background tasks
    // Read NFC tags
    SensorManager::loop();
    
    // Handle BLE notifications if connected
    BleEmulator::loop();
    
    // Give FreeRTOS/SoftDevice a moment to breathe
    delay(5);
}
