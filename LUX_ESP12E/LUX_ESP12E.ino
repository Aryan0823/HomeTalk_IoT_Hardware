#include <Wire.h>

// BH1750 I2C Address
#define BH1750_ADDRESS 0x23

// Measurement Mode
#define CONTINUOUS_HIGH_RES_MODE 0x10

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize I2C communication
  Wire.begin(4, 5);  // SDA on GPIO4, SCL on GPIO5 for ESP12F
  
  // Optional: Wait for serial to connect
  while (!Serial) {
    ; 
  }
  
  Serial.println("BH1750 Light Sensor Test");
  
  // Configure BH1750 to continuous high resolution mode
  Wire.beginTransmission(BH1750_ADDRESS);
  Wire.write(CONTINUOUS_HIGH_RES_MODE);
  Wire.endTransmission();
}

void loop() {
  // Request 2 bytes of data from BH1750
  Wire.requestFrom(BH1750_ADDRESS, 2);
  
  // Read the light intensity
  if (Wire.available() == 2) {
    unsigned int lightLevel = Wire.read() << 8 | Wire.read();
    
    // Calculate lux (light intensity)
    // BH1750 returns a 16-bit value where each count is 1 lux
    float luxValue = lightLevel / 1.2;
    
    // Print the light intensity
    Serial.print("Light Intensity: ");
    Serial.print(luxValue);
    Serial.println(" lux");
  }
  
  // Wait for a second before next reading
  delay(1000);
}