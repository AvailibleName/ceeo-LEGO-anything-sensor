#include "debug.h"
#include "ble_emulator.h"
#include "power_ui.h"
// LEGO Service and Characteristics
#define SERVICE_UUID        "0000FD02-0000-1000-8000-00805F9B34FB"
#define CHAR_RX_UUID        "0000FD02-0001-1000-8000-00805F9B34FB"
#define CHAR_TX_UUID        "0000FD02-0002-1000-8000-00805F9B34FB"

static BLEService        legoService(SERVICE_UUID);
static BLECharacteristic rxChar(CHAR_RX_UUID);
static BLECharacteristic txChar(CHAR_TX_UUID);

static bool deviceConnected = false;
static uint16_t currentConnHandle = BLE_CONN_HANDLE_INVALID;
static LegoSensor* currentSensor = nullptr;
static uint32_t lastNotifyTime = 0;
static uint32_t notifyIntervalMs = 0; // 0 means notifications disabled

void BleEmulator::connect_callback(uint16_t conn_handle) {
    deviceConnected = true;
    currentConnHandle = conn_handle;
    
    // Stop the pairing light flashing
    PowerUI::forceState(UIState::IDLE);
    
    DEBUG_PRINTLN("Client connected!");
}

void BleEmulator::disconnect_callback(uint16_t conn_handle, uint8_t reason) {
    deviceConnected = false;
    currentConnHandle = BLE_CONN_HANDLE_INVALID;
    notifyIntervalMs = 0;
    
    // Resume pairing light flashing
    PowerUI::forceState(UIState::PAIRING);
    
    DEBUG_PRINT("Client disconnected, reason = 0x");
    DEBUG_PRINTLN(reason, HEX);
}

void BleEmulator::rx_write_callback(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    if (len > 0) {
        uint8_t cmd = data[0];
        
        if (cmd == 0) { // INFO_REQUEST
            DEBUG_PRINTLN("Received INFO_REQUEST. Sending INFO_RESPONSE...");
            uint8_t fullResp[17] = {0};
            fullResp[0] = 1;
            fullResp[1] = 1; fullResp[2] = 0; fullResp[3] = 73; fullResp[4] = 0; // RPC 1.0.73
            fullResp[5] = 1; fullResp[6] = 0; fullResp[7] = 0; fullResp[8] = 0;  // FW 1.0.0
            fullResp[9] = 1; fullResp[10] = 0; fullResp[11] = 0; fullResp[12] = 0; // BL 1.0.0
            fullResp[13] = 100; fullResp[14] = 0; // maxPacketSize = 100
            
            uint16_t prodId = currentSensor ? currentSensor->getProductId() : 514;
            fullResp[15] = prodId & 0xFF; fullResp[16] = prodId >> 8; 
            
            txChar.notify(fullResp, sizeof(fullResp));
        } 
        else if (cmd == 26) { // DEVICE_UUID_REQUEST
            DEBUG_PRINTLN("Received DEVICE_UUID_REQUEST.");
            uint8_t resp[9] = {0};
            resp[0] = 27; 
            resp[1] = 0x11; resp[2] = 0x22; resp[3] = 0x33; resp[4] = 0x44;
            resp[5] = 0x55; resp[6] = 0x66; resp[7] = 0x77; resp[8] = 0x88;
            txChar.notify(resp, sizeof(resp));
        }
        else if (cmd == 40) { // DEVICE_NOTIFICATION_REQUEST
            if (len >= 3) {
                uint16_t delay = data[1] | (data[2] << 8);
                notifyIntervalMs = delay;
                DEBUG_PRINT("Received DEVICE_NOTIFICATION_REQUEST: ");
                DEBUG_PRINT(delay);
                DEBUG_PRINTLN(" ms");
                
                // CRITICAL FIX: Send DEVICE_NOTIFICATION_RESPONSE (41) back to the client
                // so it doesn't hang indefinitely waiting for an acknowledgment.
                uint8_t resp[2] = {41, 0}; // 41 = DEVICE_NOTIFICATION_RESPONSE, 0 = STATUS_ACK
                txChar.notify(resp, sizeof(resp));
            }
        }
        else {
            // Unhandled core command. Route it to the active sensor so it can parse
            // custom incoming data (e.g. text for an OLED display).
            if (currentSensor) {
                currentSensor->handleDataReceived(data, len);
            }
        }
    }
}

void BleEmulator::begin() {
    Bluefruit.begin();
    Bluefruit.setName("LEGO Sensor");
    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

    legoService.begin();

    txChar.setProperties(CHR_PROPS_NOTIFY);
    txChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    txChar.begin();

    rxChar.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
    rxChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    rxChar.setWriteCallback(rx_write_callback);
    rxChar.begin();

    DEBUG_PRINTLN("Bluefruit BLE Emulator initialized.");
}

void BleEmulator::updateManufacturerData(uint16_t productGroupDevice, uint8_t appColor, uint16_t cardSerial) {
    bool wasAdvertising = Bluefruit.Advertising.isRunning();
    if (wasAdvertising) {
        Bluefruit.Advertising.stop();
    }

    Bluefruit.Advertising.clearData();
    Bluefruit.ScanResponse.clearData();

    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();

    uint8_t mfgData[7];
    mfgData[0] = 0x97; // Company ID LSB
    mfgData[1] = 0x03; // Company ID MSB
    mfgData[2] = (productGroupDevice >> 8) & 0xFF; // Group
    mfgData[3] = productGroupDevice & 0xFF;        // Device
    mfgData[4] = appColor; 
    mfgData[5] = cardSerial & 0xFF;        // Serial LSB
    mfgData[6] = (cardSerial >> 8) & 0xFF; // Serial MSB

    // Add manufacturer data to main advertisement (total 12 bytes)
    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, mfgData, 7);
    
    // Put the 16-byte Service UUID and Name in the Scan Response packet so we don't exceed the 31-byte limit!
    Bluefruit.ScanResponse.addService(legoService);
    Bluefruit.ScanResponse.addName();

    Bluefruit.Advertising.restartOnDisconnect(true);

    if (wasAdvertising) {
        Bluefruit.Advertising.start(0);
    }
}

void BleEmulator::disconnect() {
    if (Bluefruit.connected()) {
        Bluefruit.disconnect(Bluefruit.connHandle());
    }
}

void BleEmulator::startAdvertising() {
    Bluefruit.Advertising.start(0); // 0 = Don't stop advertising after n seconds
    DEBUG_PRINTLN("Advertising started...");
}

void BleEmulator::stopAdvertising() {
    Bluefruit.Advertising.stop();
    DEBUG_PRINTLN("Advertising stopped.");
}

void BleEmulator::setSensor(LegoSensor* sensor) {
    currentSensor = sensor;
}

void BleEmulator::loop() {
    if (deviceConnected && notifyIntervalMs > 0 && currentSensor != nullptr) {
        uint32_t now = millis();
        if (now - lastNotifyTime >= notifyIntervalMs) {
            lastNotifyTime = now;
            
            uint8_t sensorData[32];
            size_t sensorLength = 0;
            currentSensor->buildNotification(sensorData, sensorLength);
            
            if (sensorLength > 0) {
                // Wrap the payload in a DEVICE_NOTIFICATION envelope (Message ID 60)
                uint8_t buffer[35];
                buffer[0] = 60; // DEVICE_NOTIFICATION
                buffer[1] = sensorLength & 0xFF; // Length LSB
                buffer[2] = (sensorLength >> 8) & 0xFF; // Length MSB
                memcpy(&buffer[3], sensorData, sensorLength);
                
                // Debug dump the exact hex bytes being sent
                static int debugCount = 0;
                if (debugCount < 3) {
                    DEBUG_PRINT("BLE TX Dump: ");
                    for (int i = 0; i < 3 + sensorLength; i++) {
                        if (buffer[i] < 16) DEBUG_PRINT("0");
                        DEBUG_PRINT(buffer[i], HEX);
                        DEBUG_PRINT(" ");
                    }
                    DEBUG_PRINTLN();
                    debugCount++;
                }

                txChar.notify(buffer, 3 + sensorLength);
            }
        }
    }
}
