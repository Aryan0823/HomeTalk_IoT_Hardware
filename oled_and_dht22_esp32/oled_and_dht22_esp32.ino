#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// OLED display settings
#define OLED_ADDR   0x3C
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// DHT22 settings/Users/aryanthakore/Documents/Arduino/ESP12E_WIFI_UserInfo_Firebase_DHT22 /ESP_WIFI_UserInfo_Firebase_DHT22/ESP_WIFI_UserInfo_Firebase_DHT22.ino
#define DHTPIN 4        // GPIO pin connected to the DHT22 sensor
#define DHTTYPE DHT22   // DHT22 (AM2302) sensor
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED display failed to initialize!");
    while (true); // Halt execution
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Initialize serial communication for debugging
  Serial.begin(115200);
  
  // Initialize DHT sensor
  dht.begin();
  
  // Display a startup message
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println("Aryan");
  display.display();
  delay(100);
}

void loop() {
  // Clear the display
  display.clearDisplay();

  // Read temperature and humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Sensor Error!");
  } else {
    // Display temperature and humidity on OLED
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Temp: ");
    display.print(temperature);
    display.println(" C");

    display.setCursor(0, 20);
    display.print("Hum: ");
    display.print(humidity);
    display.println(" %");

    // Print to Serial Monitor for debugging
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  }

  // Update the display
  display.display();

  // Wait for a second before reading again
  delay(1000);
}
