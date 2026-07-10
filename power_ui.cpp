#include "power_ui.h"

// Hardware pins
static uint8_t pinBtn, pinR, pinG, pinB;

// State
static UIState currentState = UIState::IDLE;
static uint8_t selectedSensorIndex = 0; // 0=Color Sensor, 1=Single Motor, etc.

// Callbacks
static void (*onSensorChanged)(uint8_t) = nullptr;
static void (*onPairingToggled)(bool) = nullptr;

// Card color
static uint8_t targetColorR = 0;
static uint8_t targetColorG = 0;
static uint8_t targetColorB = 255; // Default blue if no card


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
uint8_t PowerUI::getSelectedSensorIndex() { return selectedSensorIndex; }
void PowerUI::setSensorChangedCallback(void (*cb)(uint8_t)) { onSensorChanged = cb; }
void PowerUI::setPairingToggledCallback(void (*cb)(bool)) { onPairingToggled = cb; }

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
        default: targetColorR = 0;   targetColorG = 0;   targetColorB = 255; break; // Default Blue
    }
}

void PowerUI::powerOff() {
    Serial.println("Powering off (Deep Sleep)...");
    setLed(0, 0, 0);
    delay(100);

    // Prepare D0 for wakeup (low level)
    // NRF52 specific deep sleep code
#ifdef NRF52840_XXAA
    nrf_gpio_cfg_sense_input(digitalPinToPinName(pinBtn), NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
    NRF_POWER->SYSTEMOFF = 1;
#else
    // Fallback if not specifically nRF52
    while (digitalRead(pinBtn) == LOW) delay(10); // Wait for release
    while (true) {
        // Deep sleep stub
        delay(1000); 
    }
#endif
}

void PowerUI::loop() {
    handleButton();

    // LED status based on state
    if (currentState == UIState::IDLE) {
        setLed(targetColorR, targetColorG, targetColorB); 
    } else if (currentState == UIState::PAIRING) {
        // Blink current card color for pairing (or white if none)
        if ((millis() / 500) % 2 == 0) setLed(targetColorR, targetColorG, targetColorB);
        else setLed(0, 0, 0);
    } else if (currentState == UIState::CONFIG_MODE) {
        // Cycle colors based on selectedSensorIndex
        if (selectedSensorIndex == 0) setLed(255, 0, 255); // Purple = Color Sensor
        else if (selectedSensorIndex == 1) setLed(255, 0, 0); // Red = Single Motor
        else if (selectedSensorIndex == 2) setLed(0, 255, 0); // Green = Double Motor
        else setLed(255, 255, 0); // Yellow = Controller
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
                    if (pressDuration > 3000 && pressDuration < 5000 && currentState != UIState::CONFIG_MODE) {
                        powerOff();
                    }
                    else if (pressDuration <= 3000) {
                        // Short Click
                        if (currentState == UIState::IDLE) {
                            currentState = UIState::PAIRING;
                            if (onPairingToggled) onPairingToggled(true);
                        } else if (currentState == UIState::PAIRING) {
                            currentState = UIState::IDLE;
                            if (onPairingToggled) onPairingToggled(false);
                        } else if (currentState == UIState::CONFIG_MODE) {
                            selectedSensorIndex = (selectedSensorIndex + 1) % 4;
                        }
                    }
                }
            }
        }
    }

    // Handle long holds while pressed
    if (btnIsPressed && !longPressHandled) {
        uint32_t pressDuration = now - btnPressTime;
        
        if (currentState == UIState::CONFIG_MODE && pressDuration > 2000) {
            // Save and exit config mode
            currentState = UIState::IDLE;
            longPressHandled = true;
            if (onSensorChanged) onSensorChanged(selectedSensorIndex);
            
            // Blink white to confirm save
            setLed(255, 255, 255);
            delay(500);
        }
        else if (currentState != UIState::CONFIG_MODE && pressDuration > 5000) {
            // Enter Config Mode
            currentState = UIState::CONFIG_MODE;
            longPressHandled = true;
            
            // Turn off pairing if it was on
            if (onPairingToggled) onPairingToggled(false);
        }
        else if (currentState != UIState::CONFIG_MODE && pressDuration > 3000 && pressDuration <= 5000) {
            // We wait to see if they hold for 5s. If they release between 3s and 5s, power off.
            // Wait, this means power off only happens ON RELEASE between 3 and 5 sec, 
            // OR we just trigger it immediately if they hold for 3s.
            // Let's make power off trigger immediately at 3s if they are in IDLE.
            // But they need 5s to get to Config mode!
            // This is a UX conflict. A 5s hold passes through the 3s hold.
            // Alternative: Power off is 3s. Config mode is 5s. 
            // We must only trigger Power Off on *release* if duration is between 3s and 5s.
        }
    }

    // No looping logic here, it's handled on release!
    lastBtnState = reading;
}
