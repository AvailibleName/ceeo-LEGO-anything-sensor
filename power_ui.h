#ifndef POWER_UI_H
#define POWER_UI_H

#include <Arduino.h>

enum class UIState {
    IDLE,
    PAIRING,
    CONFIG_MODE
};

class PowerUI {
public:
    static void begin(uint8_t btnPin, uint8_t ledR, uint8_t ledG, uint8_t ledB);
    static void loop();
    static UIState getState();
    static void setLed(uint8_t r, uint8_t g, uint8_t b); // 0-255
    static void setCardColor(uint8_t fwColor); // Maps LEGO firmware color to RGB

    // Config Mode sensor selection (0-3 for example)
    static uint8_t getSelectedSensorIndex();
    static void setSensorChangedCallback(void (*callback)(uint8_t));
    static void setPairingToggledCallback(void (*callback)(bool));

private:
    static void handleButton();
    static void powerOff();
};

#endif
