## Bill of Materials (BOM)

To build the hardware for this device, you will need the following components:

- **Breadboard:** [Amazon Link](https://a.co/d/08REgV1U)
- **Microcontroller (Seeed XIAO nRF52840):** [Amazon Link](https://a.co/d/0hvDNsSc)
- **Battery:** [Amazon Link](https://a.co/d/0hvjeEuI)
- **RGB Button:** [Amazon Link](https://a.co/d/0b4CaRvc)
- **NFC Reader (MFRC522/WS1850S):** [Amazon Link](https://www.amazon.com/s?k=MFRC522) *(Placeholder)*
- **Misc:** Jumper wires, NFC cards/tags, and the actual sensors you wish to connect (e.g., VEML6040 Color Sensor, I2C OLED).

---

## Getting Started

### 1. Clone this Repository
First, download the repository so you have access to both the Arduino firmware and the Python library:
```bash
git clone https://github.com/AvailibleName/ceeo-LEGO-anything-sensor
cd ceeo-LEGO-anything-sensor
```
*(The Arduino code is located right in the root of this folder, and the custom Python library is in the `python_lib/` directory).*

---

## Wiring Guide

> [!IMPORTANT]
> You need to remove the housing from the NFC reader.

### Power
- **XIAO `3V3`** $\rightarrow$ Route power *directly* to the NFC Sensor VCC and RGB Button power (L1), since they are inside the housing and do not connect to the exposed breadboard. Also route `3V3` to the Breadboard VCC hookup for hot-swappable sensors.
- **XIAO `GND`** $\rightarrow$ Route ground *directly* to the NFC Sensor GND and RGB Button. Also route `GND` to the Breadboard GND hookup.

### RGB Button UI
- **XIAO `D0`** $\rightarrow$ Button contact/switch (Middle pin labeled **NO** on the switch)
- **XIAO `D1`** $\rightarrow$ Blue LED (Pin **L2** on the switch)
- **XIAO `D2`** $\rightarrow$ 100$\Omega$ resistor $\rightarrow$ Green LED (Pin **L4** on the switch)
- **XIAO `D3`** $\rightarrow$ 100$\Omega$ resistor $\rightarrow$ Red LED (Pin **L3** on the switch)
- **XIAO `3V3`** $\rightarrow$ Power (Pin **L1** on the switch)

### I2C Bus (Sensors & NFC)
- **XIAO `D4` (SDA)** $\rightarrow$ Route *separately* directly to the NFC Reader SDA, and also to the Breadboard SDA hookup.
- **XIAO `D5` (SCL)** $\rightarrow$ Route *separately* directly to the NFC Reader SCL, and also to the Breadboard SCL hookup.
*(Note: The custom sensors will be hot-swapped and their wiring will be connected to the hookups for I2C routed to the breadboard).*

---

## Build Instructions

1. **3D Printing the Housing**: 
   - Print the main body housing **button-side-down**.
   - Use **organic supports** everywhere the top plate should not need supports. (CAD files should be placed in the `CAD/` folder).
2. **Wiring**: Wire all components according to the guide above.
3. **Flashing Firmware**: 
   - **Flash the Arduino firmware onto the XIAO *before* assembling the housing.** Once assembled, the tiny reset (RST) button on the XIAO used for entering Bootloader mode will not be accessible!
4. **Assembly**: 
   - Trim the tabs at the base of the Grove connector on the NFC reader.
   - Bend the Grove port upwards so that it does not stick out too far to put the top plate on.
   - Route the cables going up to the breadboard *before* sliding the breadboard into the bottom housing (it won't be accessible after).
   - Slide all components into the bottom housing.
   - Superglue the top plate to the opening.
   - Superglue two LEGO 6x2 plates to the top, and two more LEGO 6x2 plates to the bottom.

---

## Firmware Setup

1. **Install Arduino IDE**: Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
2. **Add Board Manager URL**: Open Arduino IDE. Go to `File > Preferences` and add `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json` to the "Additional Boards Manager URLs" field.
3. **Install Board Support**: Go to `Tools > Board > Boards Manager...`, search for `Adafruit nRF52`, and install the **Adafruit nRF52 by Adafruit** package.
4. **Select Board**: Go to `Tools > Board > Adafruit nRF52 Boards` and select **Seeed XIAO nRF52840**.
5. **Install Libraries**: Go to `Tools > Manage Libraries...` and ensure you have any required libraries installed (e.g., `Adafruit TinyUSB Library` should be installed by the board package, but verify if needed).
6. **Flash the Code**: Open the `LegoSmartSensor.ino` file (located in the folder you just cloned) in the Arduino IDE, select the correct COM port under `Tools > Port`, and click the **Upload** arrow to flash the firmware to the XIAO. 

> [!TIP]
> If you ever need to flash new firmware after the device is assembled, hold the RGB button down for **10 seconds** while the device is on. The light will turn solid blue, and the device will reboot into DFU Bootloader mode, allowing you to upload code over USB.

---

## Software Setup (Python)

To communicate with the hardware, we use a fork of the `legoeducation` Python library that has been extended to support our custom sensors. For documentation on the original base library, refer to the [LEGOEducation GitHub](https://github.com/LEGO/LEGOEducation/blob/main/README.md).

### 1. Set up a Virtual Environment
We recommend creating a Python virtual environment inside the cloned folder to house the custom library:

```bash
# Create the virtual environment (use --system-site-packages if you have Bleak installed globally)
python3 -m venv --system-site-packages venv

# Activate it
source venv/bin/activate
```

### 2. Install the Custom Library
The custom library is located in the `python_lib/legoeducation` folder of this repository. You can copy it directly into your virtual environment:

```bash
cp -r python_lib/legoeducation venv/lib/python3.14/site-packages/
```
*(Make sure to adjust `python3.14` to match your actual Python version!)*

### 3. Running the Example
Once activated, run the provided boilerplate connection script to test your connection:
```bash
python3 examples/example.py
```

---

## Custom Sensor Documentation

By extending the `legoeducation` library in this repo, we can support totally custom hardware modules. 
### Color Sensor (VEML6040)
- **Product ID**: `61` (Emulates the official LEGO SPIKE Color Sensor)
- **Features**: This firmware module reads data from a VEML6040 I2C sensor and packages it to behave *identically* to an official LEGO Color Sensor.
- **Usage**: Since it acts identically to the official hardware, you can just use the standard legoeducation `ColorSensor` class. Please refer directly to the [original LEGOEducation library documentation](https://github.com/LEGO/LEGOEducation/blob/main/README.md) for full instructions on how to use it!


### OLED Display (`OledSensor`)
- **Product ID**: `530`
- **Notification ID**: `20`
- **Features**: 2-way data flow over BLE. The XIAO routes raw payload bytes directly into the C++ `handleDataReceived()` function.
- **Python Methods Available**:
  - `oled.print_text("Hello World")`: Prints standard ASCII text to the screen.
  - `oled.write_array(data)`: Renders raw 2D pixel arrays.
  - `oled._send_commands(bytes([0x06]))`: Clears the display.
  - `oled._send_commands(bytes([0x05, col, page]))`: Sets cursor position.
  - `oled._send_commands(bytes([0x03]))`: Starts hardware scroll.
  - `oled._send_commands(bytes([0x04]))`: Stops hardware scroll.

---

## Adding Your Own Custom Sensors (AI Generator)

Because adding a completely custom sensor requires both writing a C++ driver for the microcontroller AND modifying the Python `legoeducation` library on your PC to parse the data, we have included an AI Prompt generator to do the heavy lifting for you!

You can find the AI prompt template in the `AI_SENSOR_PROMPT.md` file in the root of this repository. 

To use it:
1. Open `AI_SENSOR_PROMPT.md`.
2. Choose **Option 1** if you are using an agentic AI that can read your local files (like Cursor, Claude Code, or Antigravity), or **Option 2** if you are pasting into a web chatbot (like ChatGPT or Claude Web).
3. Fill in the bracketed placeholders (`[LIKE THIS]`) with the specific I2C address and details of the hardware module you want to add.
4. The AI will instantly generate the `MySensor.cpp` and `MySensor.h` files for your Arduino firmware, AND provide the exact Python snippets you need to paste into `python_lib/legoeducation/rpc_message.py` and `device.py` to seamlessly parse the new data!
