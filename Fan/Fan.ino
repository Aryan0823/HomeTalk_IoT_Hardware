#include <Preferences.h>

// Define the GPIO pins for the fan relays
#define FAN_RELAY1 15
#define FAN_RELAY2 2
#define FAN_RELAY3 4

// Define the GPIO pins for the fan buttons
#define FAN_UP_BUTTON 33
#define FAN_DOWN_BUTTON 32


int currentSpeed = 0;
Preferences preferences;

// Debounce variables
unsigned long lastDebounceTimeUp = 0;
unsigned long lastDebounceTimeDown = 0;
unsigned long debounceDelay = 50; // 50 milliseconds

// To track button press state
bool buttonUpPressed = false;
bool buttonDownPressed = false;

void setup() {
    Serial.begin(9600);

    // Open the Preferences namespace
    preferences.begin("fan_state", false);

    // Set the fan relay pins as outputs
    pinMode(FAN_RELAY1, OUTPUT);
    pinMode(FAN_RELAY2, OUTPUT);
    pinMode(FAN_RELAY3, OUTPUT);

    // Set the fan button pins as inputs with pull-up resistors
    pinMode(FAN_UP_BUTTON, INPUT_PULLUP);
    pinMode(FAN_DOWN_BUTTON, INPUT_PULLUP);

    // Restore the last fan speed from the preferences
    currentSpeed = preferences.getInt("current_speed", 0);
    fanSpeedControl(currentSpeed);

    // Print the initial fan speed status
    printFanSpeedStatus();
}

void loop() {
    manualFanControl();
}

void manualFanControl() {
    // Read the state of the increment button
    int readingUp = digitalRead(FAN_UP_BUTTON);
    
    if (readingUp == LOW && !buttonUpPressed && (millis() - lastDebounceTimeUp) > debounceDelay) {
        if (currentSpeed < 4) {
            currentSpeed++;
            fanSpeedControl(currentSpeed);
            preferences.putInt("current_speed", currentSpeed);
            printFanSpeedStatus();
        }
        buttonUpPressed = true; // Mark that the button was pressed
        lastDebounceTimeUp = millis(); // Reset debounce timer
    } else if (readingUp == HIGH) {
        buttonUpPressed = false; // Reset when button is released
    }

    // Read the state of the decrement button
    int readingDown = digitalRead(FAN_DOWN_BUTTON);
    
    if (readingDown == LOW && !buttonDownPressed && (millis() - lastDebounceTimeDown) > debounceDelay) {
        if (currentSpeed > 0) {
            currentSpeed--;
            fanSpeedControl(currentSpeed);
            preferences.putInt("current_speed", currentSpeed);
            printFanSpeedStatus();
        }
        buttonDownPressed = true; // Mark that the button was pressed
        lastDebounceTimeDown = millis(); // Reset debounce timer
    } else if (readingDown == HIGH) {
        buttonDownPressed = false; // Reset when button is released
    }
}

void fanSpeedControl(int fanSpeed) {
    switch (fanSpeed) {
        case 0:
            digitalWrite(FAN_RELAY1, LOW);
            digitalWrite(FAN_RELAY2, LOW);
            digitalWrite(FAN_RELAY3, LOW);
            break;
        case 1:
            digitalWrite(FAN_RELAY1, HIGH);
            digitalWrite(FAN_RELAY2, LOW);
            digitalWrite(FAN_RELAY3, LOW);
            break;
        case 2:
            digitalWrite(FAN_RELAY1, LOW);
            digitalWrite(FAN_RELAY2, HIGH);
            digitalWrite(FAN_RELAY3, LOW);
            break;
        case 3:
            digitalWrite(FAN_RELAY1, HIGH);
            digitalWrite(FAN_RELAY2, HIGH);
            digitalWrite(FAN_RELAY3, LOW);
            break;
        case 4:
            digitalWrite(FAN_RELAY1, HIGH);
            digitalWrite(FAN_RELAY2, HIGH);
            digitalWrite(FAN_RELAY3, HIGH);
            break;
        default:
            break;
    }
}

void printFanSpeedStatus() {
    Serial.print("Fan speed: ");
    switch (currentSpeed) {
        case 0:
            Serial.println("Off");
            break;
        case 1:
            Serial.println("Speed 1");
            break;
        case 2:
            Serial.println("Speed 2");
            break;
        case 3:
            Serial.println("Speed 3");
            break;
        case 4:
            Serial.println("Speed 4");
            break;
        default:
            break;
    }
}