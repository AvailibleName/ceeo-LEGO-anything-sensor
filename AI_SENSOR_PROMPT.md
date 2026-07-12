# AI Prompt Generator: LEGO Smart Sensor Driver

This document contains two AI prompts depending on what kind of AI you are using.

**Before you copy either prompt, make sure to replace the bracketed placeholders `[LIKE THIS]` with your specific sensor details!**

---

## Option 1: For Agentic AI (Antigravity, Claude Code, Cursor)
Use this prompt if the AI **has access to your local filesystem** and can read your code directly.

**[COPY BELOW THIS LINE]**

I am building a custom C++ firmware for a Seeed XIAO nRF52840 that emulates a LEGO Education sensor brick (like a Color Sensor, Matrix Display, or Distance Sensor) over BLE. 

The repository is located here: **[INSERT REPOSITORY FILEPATH HERE, e.g., /home/user/LegoSmartSensor]**

Please explore the repository and read all the `.cpp`, `.h`, and `.ino` files (especially `sensor_base.h`, `ble_emulator.cpp`, and the Python code in `python_lib/legoeducation`) to fully understand how the architecture handles device auto-detection, BLE notification packing, and 2-way data routing.

I need you to write a custom sensor driver for this hardware:
- **My Sensor/Addon Name:** [INSERT YOUR SENSOR NAME/MODEL HERE, e.g., TCS34725 I2C Color Sensor]
- **My Sensor's I2C Address:** [INSERT I2C ADDRESS, e.g., 0x29]
- **The specific data I want to extract/send:** [INSERT WHAT DATA YOU WANT, e.g., "I want to extract raw RGB values and convert them to HSV."]

Please implement the C++ `.cpp` and `.h` files inheriting from `LegoSensor`, and the Python payload/device classes for the `legoeducation` library.

**CRITICAL RULES & COMMON ISSUES TO AVOID:**
1. **No I2C Timeouts:** The Adafruit nRF52 Arduino Core's `TwoWire` class does **NOT** support `Wire.setWireTimeout()`. Do not attempt to use it or the code will fail to compile.
2. **No Serial Blocking:** This device often runs on battery power disconnected from USB. Never use `while(!Serial)` or any blocking serial wait loops, as this will completely freeze the device when unplugged. Always use safe wrappers like `if (Serial) { Serial.print(...); }`.
3. **Graceful Disconnects:** Ensure your `isConnected()` function correctly catches if the device is suddenly unplugged from the I2C bus, and ensure the main `buildNotification()` loop does not hang indefinitely waiting for bytes from an unresponsive I2C sensor.
4. **Integration:** Tell me exactly what lines to add to `SensorManager::detectSensors()` and where to inject the Python code.

---

## Option 2: For Chatbots (ChatGPT, Claude Web, Gemini)
Use this prompt if the AI **does not have access to your filesystem** and needs the architecture explained to it.

**[COPY BELOW THIS LINE]**

I am building a custom C++ firmware for a Seeed XIAO nRF52840 that emulates a LEGO Education sensor brick (like a Color Sensor, Matrix Display, or Distance Sensor) over BLE. 

I need you to write a custom sensor driver class that I can drop into my firmware.

### 1. The Target Hardware
- **My Microcontroller:** Seeed XIAO nRF52840 (Arduino Core)
- **My Sensor/Addon Name:** [INSERT YOUR SENSOR NAME/MODEL HERE, e.g., TCS34725 I2C Color Sensor]
- **My Sensor's I2C Address:** [INSERT I2C ADDRESS, e.g., 0x29]
- **The specific data I want to extract/send:** [INSERT WHAT DATA YOU WANT, e.g., "I want to extract raw RGB values and convert them to HSV."]

### 2. The Firmware Architecture
The firmware is already written and has an automatic hardware-detection system. It works like this:
1. `SensorManager` continuously polls the I2C bus. If it detects a device at a specific I2C address, it swaps a global pointer to use that specific sensor's driver class.
2. The `BleEmulator` automatically calls `buildNotification()` on the active sensor 10 times a second.
3. The `BleEmulator` automatically wraps the output of `buildNotification()` into the standard LEGO `DEVICE_NOTIFICATION` (MsgID 60) envelope, so the sensor driver ONLY needs to provide the sensor-specific inner payload bytes.

### 3. The Required Interface
You must create a `.h` and `.cpp` file for a class that inherits from this exact `LegoSensor` interface:

```cpp
#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H
#include <Arduino.h>

class LegoSensor {
public:
    virtual ~LegoSensor() = default;
    virtual void begin() = 0;
    virtual uint16_t getProductId() = 0;
    virtual bool isConnected() { return true; }
    virtual void buildNotification(uint8_t* buffer, size_t& length) = 0;
    virtual void handleDataReceived(const uint8_t* payload, size_t length) {}
};
#endif
```

### 4. Critical Rules & Common Issues
1. **No I2C Timeouts:** The Adafruit nRF52 Arduino Core's `TwoWire` class does **NOT** support `Wire.setWireTimeout()`. Do not attempt to use it or the code will fail to compile.
2. **No Serial Blocking:** This device often runs on battery power disconnected from USB. Never use `while(!Serial)` or any blocking serial wait loops, as this will completely freeze the device when unplugged. Always use safe wrappers like `if (Serial) { Serial.print(...); }`.
3. **Graceful Disconnects:** Ensure your `isConnected()` function correctly catches if the device is suddenly unplugged from the I2C bus. Ensure `buildNotification()` doesn't hang in a `while` loop waiting for I2C bytes.

### Your Task
1. **Write the C++ Header (`MySensor.h`)**: Define the class inheriting from `LegoSensor`. 
2. **Write the C++ Implementation (`MySensor.cpp`)**: 
   - Implement `begin()` using standard Arduino `Wire` commands.
   - Implement `isConnected()` by doing a quick `Wire.beginTransmission(address); return (Wire.endTransmission() == 0);`.
   - Implement `buildNotification()`. The very first byte `buffer[0]` MUST be a unique Message ID (e.g., 20) that we will define for this sensor.
   - **(Optional for 2-way data)**: Override `virtual void handleDataReceived(const uint8_t* payload, size_t length)` to parse incoming commands from the host PC.
3. **Write the Python `rpc_message.py` additions**:
   - Provide the Python code to define the `PRODUCT_GROUP_DEVICE_[NAME]` constant.
   - Provide the `[NAME]_NOTIFICATION = 20` constant.
   - Provide the `class [Name]Notification:` Python class that uses the `struct` module to unpack the exact byte payload you designed in step 2.
4. **Write the Python `device.py` additions**:
   - Provide the `class [Name](_BasicDevice):` Python class. 
   - Set `self.product_id` to your new constant.
   - Add `@property` getters to easily read the unpacked data from `self.sensor`.
   - **(Optional for 2-way data)**: Add Python methods (e.g., `def print_text(self, text):`) that use `self.send(struct.pack(...))` to blast the data directly to the Xiao over Bluetooth.
5. **Integration Instructions**: Briefly explain where to paste the C++ code (in `SensorManager::detectSensors()`) and where to paste the Python code (in the `legoeducation` library files).
