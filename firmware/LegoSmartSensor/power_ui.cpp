#include "debug.h"
#include "power_ui.h"

// Hardware pins
static uint8_t pinBtn, pinR, pinG, pinB;

// State
static UIState currentState = UIState::IDLE;
static void (*onPairingToggled)(bool) = nullptr;

// Card color
static uint8_t targetColorR = 255;
static uint8_t targetColorG = 255;
static uint8_t targetColorB = 255; // Default white if no card


// Button timing
static uint32_t btnPressTime = 0;
static bool btnIsPressed = false;
static bool longPressHandled = false;

// Debouncing
static uint32_t lastDebounceTime = 0;
static bool lastBtnState = HIGH; 
static bool currentBtnState = HIGH;

void PowerUI::begin(uint8_t btnPin, uint8_t ledR, uint8_t ledG, uint8_t ledB) {
    pinBtn = btnPin;
    pinR = ledR;
    pinG = ledG;
    pinB = ledB;

    pinMode(pinBtn, INPUT_PULLUP);
    pinMode(pinR, OUTPUT);
    pinMode(pinG, OUTPUT);
    pinMode(pinB, OUTPUT);
    
    // Default to Off (Common Anode: HIGH = off)
    setLed(0, 0, 0);
}

UIState PowerUI::getState() { return currentState; }
void PowerUI::setPairingToggledCallback(void (*cb)(bool)) { onPairingToggled = cb; }

void PowerUI::forceState(UIState state) {
    currentState = state;
}

void PowerUI::setLed(uint8_t r, uint8_t g, uint8_t b) {
    // Common anode: 255 - value for PWM
    analogWrite(pinR, 255 - r);
    analogWrite(pinG, 255 - g);
    analogWrite(pinB, 255 - b);
}

void PowerUI::setCardColor(uint8_t fwColor) {
    // 0=BLACK, 1=MAGENTA, 2=PURPLE, 3=BLUE, 4=AZURE, 5=TURQUOISE, 
    // 6=GREEN, 7=YELLOW, 8=ORANGE, 9=RED, 10=WHITE, 255=DEFAULT(Blue)
    switch(fwColor) {
        case 1:  targetColorR = 255; targetColorG = 0;   targetColorB = 255; break; // Magenta
        case 2:  targetColorR = 128; targetColorG = 0;   targetColorB = 128; break; // Purple
        case 3:  targetColorR = 0;   targetColorG = 0;   targetColorB = 255; break; // Blue
        case 4:  targetColorR = 0;   targetColorG = 128; targetColorB = 255; break; // Azure
        case 5:  targetColorR = 64;  targetColorG = 224; targetColorB = 208; break; // Turquoise
        case 6:  targetColorR = 0;   targetColorG = 255; targetColorB = 0;   break; // Green
        case 7:  targetColorR = 255; targetColorG = 255; targetColorB = 0;   break; // Yellow
        case 8:  targetColorR = 255; targetColorG = 165; targetColorB = 0;   break; // Orange
        case 9:  targetColorR = 255; targetColorG = 0;   targetColorB = 0;   break; // Red
        case 10: targetColorR = 255; targetColorG = 255; targetColorB = 255; break; // White
        case 0:
        default: targetColorR = 255; targetColorG = 255; targetColorB = 255; break; // Default White
    }
}

void PowerUI::powerOff() {
    DEBUG_PRINTLN("Powering off (Deep Sleep)...");
    
    // Flash Red briefly to indicate power off
    setLed(255, 0, 0);
    delay(200);
    setLed(0, 0, 0);

    // CRITICAL: We must wait for the user to RELEASE the button!
    // If we enter deep sleep while the button is still held down, 
    // the SENSE_LOW condition is immediately met and the device wakes back up instantly!
    while (digitalRead(pinBtn) == LOW) {
        delay(10);
    }
    delay(50); // Small debounce

    // Prepare D0 for wakeup (low level)
    // NRF52 specific deep sleep code
#ifdef NRF52840_XXAA
    nrf_gpio_cfg_sense_input(digitalPinToPinName(pinBtn), NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
    NRF_POWER->SYSTEMOFF = 1;
#else
    // Fallback if not specifically nRF52
    while (true) {
        // Deep sleep stub
        delay(1000); 
    }
#endif
}

void PowerUI::loop() {
    handleButton();

    // Don't overwrite the LED if the user is holding the button for a long press
    if (btnIsPressed && (millis() - btnPressTime > 3000)) {
        return;
    }

    // LED status based on state
    if (currentState == UIState::IDLE) {
        setLed(targetColorR, targetColorG, targetColorB); 
    } else if (currentState == UIState::PAIRING) {
        // Blink current card color for pairing (or white if none)
        if ((millis() / 500) % 2 == 0) setLed(targetColorR, targetColorG, targetColorB);
        else setLed(0, 0, 0);
    }
}

void PowerUI::handleButton() {
    bool reading = digitalRead(pinBtn);
    uint32_t now = millis();

    if (reading != lastBtnState) {
        lastDebounceTime = now;
    }

    if ((now - lastDebounceTime) > 50) {
        if (reading != currentBtnState) {
            currentBtnState = reading;

            if (currentBtnState == LOW) { // Pressed
                btnPressTime = now;
                btnIsPressed = true;
                longPressHandled = false;
            } else { // Released
                btnIsPressed = false;
                uint32_t pressDuration = now - btnPressTime;

                if (!longPressHandled && pressDuration > 50) {
                    // Short Click
                    if (pressDuration <= 3000) {
                        if (currentState == UIState::IDLE) {
                            currentState = UIState::PAIRING;
                            if (onPairingToggled) onPairingToggled(true);
                        } else if (currentState == UIState::PAIRING) {
                            currentState = UIState::IDLE;
                            if (onPairingToggled) onPairingToggled(false);
                        }
                    } 
                    // Long press released between 3 and 10 seconds
                    else if (pressDuration <= 10000) {
                        longPressHandled = true;
                        powerOff();
                    }
                }
            }
        }
    }

    // Handle long holds while pressed
    if (btnIsPressed && !longPressHandled) {
        uint32_t pressDuration = now - btnPressTime;
        
        // Reboot to DFU Bootloader after 10 seconds
        if (pressDuration > 10000) {
            longPressHandled = true;
            DEBUG_PRINTLN("Rebooting to DFU Bootloader...");
            setLed(0, 0, 255); // Solid Blue
            delay(500);
            
#ifdef NRF52840_XXAA
            // Adafruit nRF52 bootloader magic byte for UF2
            NRF_POWER->GPREGRET = 0x57;
            NVIC_SystemReset();
#endif
        }
        // Visual indicator at 3 seconds that power-off is armed
        else if (pressDuration > 3000) {
            setLed(255, 0, 0); // Solid Red indicates ready to power off
        }
    }

    lastBtnState = reading;
}
