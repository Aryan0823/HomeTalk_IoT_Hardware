#define LED_PIN 32        // LED connected to GPIO 12
#define RELAY_PIN 14      // Relay connected to GPIO 14
#define BUTTON_PIN 13     // Push button connected to GPIO 13
#define POT_PIN 26        // Potentiometer connected to GPIO 27

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
    potValue = analogRead(POT_PIN); // Read potentiometer value (0-4095)
    int ledBrightness = map(potValue, 0, 4095, 0, 255); // Map pot value to LED brightness
    // int ledb = 255 - ledBrightness;
    analogWrite(LED_PIN, ledBrightness); // Set LED brightness

    if (digitalRead(BUTTON_PIN) == LOW && !buttonPressed) { // Check if button is pressed (LOW when pressed)
        relayState = !relayState; // Toggle relay state
        digitalWrite(RELAY_PIN, relayState); // Set relay state
        buttonPressed = true; // Mark button as pressed
        
        // Print immediately when relay state changes
        Serial.print("Button Pressed! New Relay State: ");
        Serial.println(relayState ? "ON" : "OFF");
        
        delay(50); // Debounce delay
    } else if (digitalRead(BUTTON_PIN) == HIGH) {
        buttonPressed = false; // Reset button pressed state when released
    }

    // Print values every PRINT_INTERVAL milliseconds
    if (millis() - lastPrintTime >= PRINT_INTERVAL) {
        Serial.print("Pot Value: ");
        Serial.print(potValue);
        Serial.print(" | LED Brightness: ");
        Serial.print(map(potValue, 0, 4095, 0, 100)); // Convert to percentage
        Serial.print("% | Relay: ");
        Serial.println(relayState ? "ON" : "OFF");
        
        lastPrintTime = millis();
    }

    delay(100); // Small delay for loop stability
}