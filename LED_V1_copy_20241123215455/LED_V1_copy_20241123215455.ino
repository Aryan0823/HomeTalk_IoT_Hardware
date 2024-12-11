int potPin = 34;      // Analog pin connected to the potentiometer
int ledPin = 32;      // PWM pin connected to the LED
int potValue = 0;     // Variable to store potentiometer value
int ledBrightness = 0; // Variable for mapped LED brightness

void setup() {
  pinMode(ledPin, OUTPUT);  // Set LED pin as output
  pinMode(potPin, INPUT);   // Set potentiometer pin as input

  Serial.begin(9600);       // Initialize Serial Monitor
}

void loop() {
  // Read the potentiometer value (0 to 4095 for ESP32)
  potValue = analogRead(potPin);

  // Map potentiometer value to LED brightness (0 to 255)
  ledBrightness = map(potValue, 0, 4095, 0, 255);

  // Write the brightness value to the LED
  ledcWrite(0, ledBrightness);  // Channel 0 for LED PWM control

  // Print the values to Serial Monitor
  Serial.print("Potentiometer Value: ");
  Serial.print(potValue);
  Serial.print(" | LED Brightness: ");
  Serial.println(ledBrightness);

  delay(10);  // Short delay for stability
}

// Helper function for ESP32 PWM setup
void setupPWM() {
  // Attach PWM to the LED pin (using channel 0)
  ledcAttachPin(ledPin, 0);
  // Configure the PWM frequency and resolution
  ledcSetup(0, 5000, 8); // Channel 0, 5 kHz, 8-bit resolution
}

void setup() {
  setupPWM();  // Initialize PWM
  Serial.begin(115200); // Initialize Serial Monitor
}
