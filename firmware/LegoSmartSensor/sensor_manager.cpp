#include "debug.h"
#include "sensor_manager.h"
#include "power_ui.h"

#define RST_PIN D6 // You can change this to match your RST wiring!
static MFRC522_I2C mfrc522(0x28, RST_PIN, &Wire); 

static IdleSensor idleSensor;
static VEML6040ColorSensor vemlSensor;
#include "OledSensor.h"
static OledSensor oledSensor;
static LegoSensor* activeSensor = &idleSensor; // Default



// Card state
static uint8_t currentCardColor = 0; // LEGO_COLOR_NOCOLOR
static uint16_t currentCardSerial = 0;
static uint32_t lastNfcRead = 0;

void SensorManager::begin() {
    mfrc522.PCD_Init();
    
    byte v = mfrc522.PCD_ReadRegister(MFRC522_I2C::VersionReg);
    if (v == 0x00 || v == 0xFF) {
        DEBUG_PRINTLN("WARNING: Communication failure with WS1850S.");
    } else {
        DEBUG_PRINT("Found WS1850S (MFRC522). Firmware Version: 0x");
        DEBUG_PRINTLN(v, HEX);
    }

    activeSensor->begin();
    
    // Default Advertisement setup
    BleEmulator::setSensor(activeSensor);
    BleEmulator::updateManufacturerData(activeSensor->getProductId(), currentCardColor, currentCardSerial);
}

void SensorManager::loop() {
    uint32_t now = millis();
    // Poll NFC every 1 second
    if (now - lastNfcRead > 1000) {
        lastNfcRead = now;
        readNfcCard();
        
        // Scan for I2C sensor changes
        detectSensors();
    }
}

void SensorManager::detectSensors() {
    LegoSensor* newSensor = &idleSensor;

    // Check for VEML6040 (0x10)
    Wire.beginTransmission(0x10);
    if (Wire.endTransmission() == 0) {
        newSensor = &vemlSensor;
    }
    // Check for OLED (0x3C)
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
        newSensor = &oledSensor;
    }
    // (Add more sensors here in the future: if (0x11) newSensor = &motor, etc)

    // Fallback: If active sensor is suddenly disconnected, default to Idle
    if (!activeSensor->isConnected()) {
        newSensor = &idleSensor;
    }

    if (newSensor != activeSensor) {
        DEBUG_PRINTLN("Hardware swap detected! Updating BLE...");
        activeSensor = newSensor;
        activeSensor->begin();

        // Drop active Bluetooth connections to force the client to re-read the Manufacturer Data
        BleEmulator::disconnect();
        
        // Update BLE advertisement
        BleEmulator::setSensor(activeSensor);
        BleEmulator::updateManufacturerData(activeSensor->getProductId(), currentCardColor, currentCardSerial);
    }
}

bool SensorManager::readNfcCard() {
    // Look for new cards
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return false;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
        return false;
    }

    // Buffer to store the read data
    byte buffer4[18];
    byte buffer5[18];
    byte size = sizeof(buffer4);

    // Read Page 4
    byte status;
    status = mfrc522.MIFARE_Read(4, buffer4, &size);
    if (status != MFRC522_I2C::STATUS_OK) {
        mfrc522.PICC_HaltA(); // Put card to sleep
        return false;
    }

    // Read Page 5
    size = sizeof(buffer5);
    status = mfrc522.MIFARE_Read(5, buffer5, &size);
    if (status != MFRC522_I2C::STATUS_OK) {
        mfrc522.PICC_HaltA(); // Put card to sleep
        return false;
    }

    // Put card to sleep so we don't spam reads
    mfrc522.PICC_HaltA();

    // Check for "L3G0" magic bytes on page 4
    if (buffer4[0] == 0x4C && buffer4[1] == 0x33 && buffer4[2] == 0x47 && buffer4[3] == 0x30) {
        uint8_t color = buffer5[1];
        uint16_t serial = (buffer5[2] << 8) | buffer5[3];
        
        if (color != currentCardColor || serial != currentCardSerial) {
            currentCardColor = color;
            currentCardSerial = serial;
            DEBUG_PRINTF("LEGO Card updated! Color: %d, Serial: %d\n", color, serial);
            
            PowerUI::setCardColor(currentCardColor);

            // Update BLE Advertisement so the Python app sees the new card
            BleEmulator::updateManufacturerData(activeSensor->getProductId(), currentCardColor, currentCardSerial);
        }
        return true;
    }

    return false;
}
