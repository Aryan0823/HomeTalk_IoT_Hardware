#include <DHT.h>

// Define DHT parameters
#define DHTPIN 2        // DHT22 connected to GPIO8 (D8)
#define DHTTYPE DHT22    // DHT 22 (AM2302)

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();  // Start DHT sensor
  Serial.println("DHT22 Sensor Setup Complete!");
}

void loop() {
  // Read temperature and humidity
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Check if any readings failed
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Print values to the Serial Monitor
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print("%  Temperature: ");
  Serial.print(temperature);
  Serial.println("°C");

  delay(2000);  // Wait 2 seconds between readings
}
