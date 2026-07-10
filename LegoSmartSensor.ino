#include <Arduino.h>
#include <Wire.h>
#include "power_ui.h"
#include "sensor_manager.h"
#include "ble_emulator.h"

// Define Hardware Pins for Seeed XIAO nRF52840
#define BTN_PIN D0
#define LED_R D1
#define LED_G D2
#define LED_B D3

void setup() {
    Serial.begin(115200);
    // while (!Serial) delay(10); // Wait for Serial (optional, block boot if USB not connected)
    
    Serial.println("LEGO Smart Sensor Emulator starting...");

    // Initialize Wire for I2C (default SDA=D4, SCL=D5 on XIAO)
    Wire.begin();

    // Initialize UI and Power Management
    PowerUI::begin(BTN_PIN, LED_R, LED_G, LED_B);

    // Initialize Sensor Manager and PN532
    SensorManager::begin();

    // Initialize BLE Emulator
    BleEmulator::begin();

    // Wire up UI callbacks
    PowerUI::setSensorChangedCallback([](uint8_t index) {
        Serial.printf("UI selected sensor index: %d\n", index);
        SensorManager::setManualSensorType(index);
    });

    PowerUI::setPairingToggledCallback([](bool pairingOn) {
        if (pairingOn) {
            BleEmulator::startAdvertising();
        } else {
            BleEmulator::stopAdvertising();
        }
    });

    Serial.println("System Ready.");
}

void loop() {
    // 1. Process Button and UI State Machine
    PowerUI::loop();
    
    UIState state = PowerUI::getState();

    // 2. If not in Config Mode, run background tasks
    if (state != UIState::CONFIG_MODE) {
        // Read NFC tags
        SensorManager::loop();
        
        // Handle BLE notifications if connected
        BleEmulator::loop();
    }
    
    // Give FreeRTOS/SoftDevice a moment to breathe
    delay(5);
}
