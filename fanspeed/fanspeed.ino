// Define pin numbers
const int buttonUpPin = 34;    // Pin for the increment button
const int buttonDownPin = 35;  // Pin for the decrement button

// Variable to hold the current value
int currentValue = 0;

// Variable to hold previous button states for debounce
int lastButtonUpState = LOW;
int lastButtonDownState = LOW;

// Debounce timing variables
unsigned long lastDebounceTimeUp = 0;
unsigned long lastDebounceTimeDown = 0;
unsigned long debounceDelay = 50; // milliseconds

// To track button press state
bool buttonUpPressed = false;
bool buttonDownPressed = false;

void setup() {
    // Initialize serial communication at 9600 baud rate
    Serial.begin(9600);

    // Set button pins as input with pull-down resistors
    pinMode(buttonUpPin, INPUT);
    pinMode(buttonDownPin, INPUT);
    
    // Print initial value
    Serial.print("Current Value: ");
    Serial.println(currentValue);
}

void loop() {
    // Read the state of the increment button
    int readingUp = digitalRead(buttonUpPin);
    
    if (readingUp != lastButtonUpState) {
        lastDebounceTimeUp = millis(); // Reset debounce timer
    }
    
    if ((millis() - lastDebounceTimeUp) > debounceDelay) {
        if (readingUp == HIGH && !buttonUpPressed) {
            // Increment value if it's less than 4
            if (currentValue < 4) {
                currentValue++;
                Serial.print("Current Value: ");
                Serial.println(currentValue);
            }
            buttonUpPressed = true; // Mark that the button was pressed
        } else if (readingUp == LOW) {
            buttonUpPressed = false; // Reset when button is released
        }
    }
    
    lastButtonUpState = readingUp; // Store the current reading

    // Read the state of the decrement button
    int readingDown = digitalRead(buttonDownPin);
    
    if (readingDown != lastButtonDownState) {
        lastDebounceTimeDown = millis(); // Reset debounce timer
    }
    
    if ((millis() - lastDebounceTimeDown) > debounceDelay) {
        if (readingDown == HIGH && !buttonDownPressed) {
            // Decrement value if it's greater than 0
            if (currentValue > 0) {
                currentValue--;
                Serial.print("Current Value: ");
                Serial.println(currentValue);
            }
            buttonDownPressed = true; // Mark that the button was pressed
        } else if (readingDown == LOW) {
            buttonDownPressed = false; // Reset when button is released
        }
    }

    lastButtonDownState = readingDown; // Store the current reading

    // Small delay to avoid bouncing issues further
    delay(10);
}