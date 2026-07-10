#include "ble_emulator.h"

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
    Serial.println("Client connected!");
}

void BleEmulator::disconnect_callback(uint16_t conn_handle, uint8_t reason) {
    deviceConnected = false;
    currentConnHandle = BLE_CONN_HANDLE_INVALID;
    notifyIntervalMs = 0;
    Serial.print("Client disconnected, reason = 0x");
    Serial.println(reason, HEX);
}

void BleEmulator::rx_write_callback(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    if (len > 0) {
        uint8_t cmd = data[0];
        
        if (cmd == 0) { // INFO_REQUEST
            Serial.println("Received INFO_REQUEST. Sending INFO_RESPONSE...");
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
            Serial.println("Received DEVICE_UUID_REQUEST.");
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
                Serial.print("Received DEVICE_NOTIFICATION_REQUEST: ");
                Serial.print(delay);
                Serial.println(" ms");
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

    Serial.println("Bluefruit BLE Emulator initialized.");
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

    Bluefruit.Advertising.restartOnDisconnect(false);

    if (wasAdvertising) {
        Bluefruit.Advertising.start(0);
    }
}

void BleEmulator::startAdvertising() {
    Bluefruit.Advertising.start(0); // 0 = Don't stop advertising after n seconds
    Serial.println("Advertising started...");
}

void BleEmulator::stopAdvertising() {
    Bluefruit.Advertising.stop();
    Serial.println("Advertising stopped.");
}

void BleEmulator::setSensor(LegoSensor* sensor) {
    currentSensor = sensor;
}

void BleEmulator::loop() {
    if (deviceConnected && notifyIntervalMs > 0 && currentSensor != nullptr) {
        uint32_t now = millis();
        if (now - lastNotifyTime >= notifyIntervalMs) {
            lastNotifyTime = now;
            
            uint8_t buffer[32];
            size_t length = 0;
            currentSensor->buildNotification(buffer, length);
            
            if (length > 0) {
                txChar.notify(buffer, length);
            }
        }
    }
}
