#include <Wire.h>
#include <DHT.h>

// Pin Definitions
#define DHTPIN 2        // DHT22 connected to GPIO6
#define DHTTYPE DHT22   // DHT 22 sensor type

// BH1750 I2C Address
#define BH1750_ADDRESS 0x23

// Measurement Mode
#define CONTINUOUS_HIGH_RES_MODE 0x10

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize I2C communication
  Wire.begin(4, 5);  // SDA on GPIO4, SCL on GPIO5 for ESP12F
  
  // Initialize DHT22 sensor
  dht.begin();
  
  // Optional: Wait for serial to connect
  while (!Serial) {
    ; 
  }
  
  Serial.println("BH1750 Light Sensor and DHT22 Sensor Test");
  
  // Configure BH1750 to continuous high resolution mode
  Wire.beginTransmission(BH1750_ADDRESS);
  Wire.write(CONTINUOUS_HIGH_RES_MODE);
  Wire.endTransmission();
}

void loop() {
  // Read light intensity from BH1750
  float luxValue = readLightIntensity();
  
  // Read temperature and humidity from DHT22
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Check if any reads failed and exit early (to try again)
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(1000);
    return;
  }
  
  // Print sensor readings
  Serial.println("--- Sensor Readings ---");
  Serial.print("Light Intensity: ");
  Serial.print(luxValue);
  Serial.println(" lux");
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  Serial.println("----------------------");
  
  // Wait for a second before next reading
  delay(2000);
}

float readLightIntensity() {
  // Request 2 bytes of data from BH1750
  Wire.requestFrom(BH1750_ADDRESS, 2);
  
  // Read the light intensity
  if (Wire.available() == 2) {
    unsigned int lightLevel = Wire.read() << 8 | Wire.read();
    
    // Calculate lux (light intensity)
    // BH1750 returns a 16-bit value where each count is 1 lux
    return lightLevel / 1.2;
  }
  
  return -1; // Return -1 if reading failed
}