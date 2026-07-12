#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

// Safe logging macros that prevent the device from hanging if unplugged from USB.
// The Adafruit nRF52 core explicitly defines `Serial` as a boolean operator that 
// evaluates to true ONLY if the USB CDC connection is fully active and the DTR line is asserted.
#define DEBUG_PRINT(...) if(Serial) { Serial.print(__VA_ARGS__); }
#define DEBUG_PRINTLN(...) if(Serial) { Serial.println(__VA_ARGS__); }
#define DEBUG_PRINTF(...) if(Serial) { Serial.printf(__VA_ARGS__); }

#endif
