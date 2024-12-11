// Pin definitions for ESP8266
#define LED_PIN D4        // Built-in LED is on GPIO2 (D4)
#define RELAY_PIN D5      // Relay connected to GPIO14 (D5)
#define BUTTON_PIN D7     // Push button connected to GPIO13 (D7)
#define POT_PIN A0        // Potentiometer connected to analog pin A0 (ADC)

// Constants for 10K potentiometer mapping
const int POT_MIN_VALUE = 0;      // Minimum value from ADC
const int POT_MAX_VALUE = 1023;   // Maximum value from ADC (ESP8266 ADC is 10-bit)
const int POT_10K_MAX = 1023;     // Maximum value for 10K pot mapping

int relayState = LOW;      // Initial state of the relay
bool buttonPressed = false; // Button press state
int potValue = 0;          // Variable to store potentiometer value
unsigned long lastPrintTime = 0;  // Time of last serial print
const int PRINT_INTERVAL = 10;  // Print interval in milliseconds

void setup() {
    pinMode(LED_PIN, OUTPUT);      // Set LED pin as output
    pinMode(RELAY_PIN, OUTPUT);    // Set relay pin as output
    pinMode(BUTTON_PIN, INPUT_PULLUP);  // Set button pin as input with internal pull-up resistor
    Serial.begin(115200);          // Start serial communication for debugging
    Serial.println("System Started");
    Serial.println("----------------");
    Serial.println("Format: Potentiometer Value | LED Brightness | Relay State");
}

void loop() {
    potValue = analogRead(POT_PIN); // Read potentiometer value (0-1023)
    
    // Map the ADC value directly to LED brightness
    // No need for first mapping step since ESP8266 ADC is already 10-bit
    int ledBrightness = map(potValue, POT_MIN_VALUE, POT_MAX_VALUE, 0, 255);
    
    // Invert the brightness value
    int ledb = 255 - ledBrightness;
    
    analogWrite(LED_PIN, ledb); // Set LED brightness

    if (digitalRead(BUTTON_PIN) == LOW && !buttonPressed) {
        relayState = !relayState;
        digitalWrite(RELAY_PIN, relayState);
        buttonPressed = true;
        
        Serial.print("Button Pressed! New Relay State: ");
        Serial.println(relayState ? "ON" : "OFF");
        
        delay(50); // Debounce delay
    } else if (digitalRead(BUTTON_PIN) == HIGH) {
        buttonPressed = false;
    }

    // Print values every PRINT_INTERVAL milliseconds
    if (millis() - lastPrintTime >= PRINT_INTERVAL) {
        Serial.print("Pot Value: ");
        Serial.print(potValue);
        Serial.print(" | LED Brightness: ");
        Serial.print(map(potValue, 0, POT_MAX_VALUE, 0, 100)); // Convert to percentage
        Serial.print("% | Relay: ");
        Serial.println(relayState ? "ON" : "OFF");
        
        lastPrintTime = millis();
    }

    delay(100);
}