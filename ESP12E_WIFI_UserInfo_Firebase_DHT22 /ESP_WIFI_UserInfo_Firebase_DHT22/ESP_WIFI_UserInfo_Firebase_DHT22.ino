#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <ESP8266HTTPClient.h>
#include <time.h>

// DHT22 configuration
#define DHTPIN D6
#define DHTTYPE DHT22
DHTesp dht;

// Firebase configuration
#define API_KEY "AIzaSyDqLcufSIP87WT1aEyDI-dKVdTsSBACrEc"
#define FIREBASE_PROJECT_ID "interactive-iot-devices-ssrip"

// Variables to store credentials
String ssid = "";
String passphrase = "";
String deviceName = "";
String category = "";
String uid = "";
String userEmail = "";
String userPassword = "";

// Static IP configuration
IPAddress local_IP(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// Button and LED pins
const int buttonPin = 5;
const int ledPin = D7;
const int LED2 = D8;

// NTP Server for time synchronization
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;  // GMT+5:30 for IST
const int   daylightOffset_sec = 0;

// Establishing Local server at port 80
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(512);
  delay(10);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(LED2, OUTPUT);
  Serial.println("Startup");

  // Initialize DHT with ESP8266 specific method
  dht.setup(DHTPIN, DHTesp::DHT22);

  readEEPROM();
  Serial.println("Stored credentials:");
  Serial.println("SSID: " + ssid);
  Serial.println("Device Name: " + deviceName);
  Serial.println("Category: " + category);
  Serial.println("UID: " + uid);
  Serial.println("User Email: " + userEmail);
  Serial.println("User Password length: " + String(userPassword.length()));

  if (ssid.length() > 0 && passphrase.length() > 0 && deviceName.length() > 0 && 
      category.length() > 0 && uid.length() > 0 && userEmail.length() > 0 && userPassword.length() > 0) {
    digitalWrite(ledPin, HIGH);
    WiFi.begin(ssid.c_str(), passphrase.c_str());
    if (testWifi()) {
      Serial.println("Successfully Connected!!!");
      digitalWrite(ledPin, LOW);
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      printLocalTime();
      handleRequests();
      return;
    }
  }
  Serial.println("Turning the HotSpot On");
  setupAP();
  handleRequests();
}

unsigned long previousMillisStatusCheck = 0;
const long intervalStatusCheck = 1000;

unsigned long previousMillisDataCheck = 0;
const long intervalDataCheck = 60000;

void loop() {
  unsigned long currentMillis = millis();

  // Check for button press to clear EEPROM
  if (digitalRead(buttonPin) == LOW) {
    Serial.println("Button pressed! Clearing EEPROM...");
    clearEEPROM();
    blinkLED(); // Visual feedback that EEPROM was cleared
    ESP.restart();
  }

  // Check device status every second if we have valid credentials
  if (currentMillis - previousMillisStatusCheck >= intervalStatusCheck) {
    previousMillisStatusCheck = currentMillis;
    if (hasValidCredentials()) {
      checkDeviceStatus();
    }
  }

  // Check and send data every minute if WiFi is connected and we have valid credentials
  if (WiFi.status() == WL_CONNECTED && hasValidCredentials()) {
    if (currentMillis - previousMillisDataCheck >= intervalDataCheck) {
      previousMillisDataCheck = currentMillis;

      TempAndHumidity lastValues = dht.getTempAndHumidity();
      float humidity = lastValues.humidity;
      float temperature = lastValues.temperature;

      if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Failed to read from DHT sensor!");
      } else {
        Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
        sendToFirestore(temperature, humidity);
      }
    }
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, HIGH);
  }

  server.handleClient();
}


bool hasValidCredentials() {
  return (uid.length() > 0 && category.length() > 0 && deviceName.length() > 0);
}

void blinkLED() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
  }
}

void checkDeviceStatus() {
  if (!hasValidCredentials()) {
    Serial.println("No valid credentials. Skipping device status check.");
    return;
  }

  String documentPath = "Data/" + uid + "/" + category + "/" + deviceName;

  String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/" + documentPath;

  // Create WiFiClientSecure object
  WiFiClientSecure client;
  client.setInsecure(); // Use with caution - only for testing

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  String idToken = getFirebaseToken();
  if (idToken.length() < 100) {
    Serial.println("Invalid Firebase ID token. Length: " + String(idToken.length()));
    return;
  }

  http.addHeader("Authorization", "Bearer " + idToken);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response body: " + response);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    // Accessing the deviceStatus field directly
    String deviceStatus = doc["fields"]["deviceStatus"]["stringValue"];
    Serial.println("Device Status: " + deviceStatus);

    if (deviceStatus == "ON") {
      digitalWrite(LED2, HIGH);
      Serial.println("LED2 turned on");
    } else if (deviceStatus == "OFF") {
      digitalWrite(LED2, LOW);
      Serial.println("LED2 turned off");
    } else {
      Serial.println("Invalid device status");
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void sendToFirestore(float temperature, float humidity) {
  if (!hasValidCredentials()) {
    Serial.println("No valid credentials. Skipping data send to Firestore.");
    return;
  }
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  char dateStr[11];
  char timeStr[9];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  String documentPath = "Data/" + uid + "/" + category + "/" + deviceName + "/" + String(dateStr);
  String documentId = String(timeStr);

  if (uid.length() == 0 || category.length() == 0 || deviceName.length() == 0) {
    Serial.println("Invalid document path: " + documentPath);
    return;
  }

  String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/" + documentPath + "?documentId=" + documentId;

  // Create WiFiClientSecure object
  WiFiClientSecure client;
  client.setInsecure(); // Use with caution - only for testing

  HTTPClient http;
  http.begin(client, url);
  
  http.addHeader("Content-Type", "application/json");
  String idToken = getFirebaseToken();
  if (idToken.length() < 100) {
    Serial.println("Invalid Firebase ID token. Length: " + String(idToken.length()));
    return;
  }
  http.addHeader("Authorization", "Bearer " + idToken);

  String jsonPayload = "{\"fields\": {\"Date\": {\"stringValue\": \"" + String(dateStr) + 
                       "\"}, \"Time\": {\"stringValue\": \"" + String(timeStr) + 
                       "\"}, \"Temperature\": {\"doubleValue\": " + String(temperature) + 
                       "}, \"Humidity\": {\"doubleValue\": " + String(humidity) + "}}}";

  Serial.println("Sending request to URL: " + url);
  Serial.println("Request payload: " + jsonPayload);

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response body: " + response);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

String getFirebaseToken() {
  // Create WiFiClientSecure object
  WiFiClientSecure client;
  client.setInsecure(); // Use with caution - only for testing
  
  HTTPClient http;
  
  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(API_KEY);
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  Serial.println("Attempting to authenticate with email: " + userEmail);
  String jsonPayload = "{\"email\":\"" + userEmail + "\",\"password\":\"" + userPassword + "\",\"returnSecureToken\":true}";
  
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Auth Response: " + response);
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return "";
    }
    String idToken = doc["idToken"].as<String>();
    if (idToken.length() == 0) {
      Serial.println("No idToken in response");
    } else {
      Serial.println("Got idToken of length: " + String(idToken.length()));
    }
    return idToken;
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    return "";
  }
  
  http.end();
}

bool testWifi(void) {
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while (c < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

void setupAP(void) {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(("SSRIP_outdoor" + deviceName).c_str(), "");
  Serial.println("Setting up AP");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void handleRequests() {
  server.on("/scan", HTTP_POST, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();
    
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
      JsonObject obj = array.createNestedObject();
      obj["ssid"] = WiFi.SSID(i);
      obj["rssi"] = WiFi.RSSI(i);
      obj["secure"] = WiFi.encryptionType(i);
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  server.on("/setting", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String qsid = server.arg("ssid");
    String qpass = server.arg("pass");
    String qdevice = server.arg("deviceName");
    String qcategory = server.arg("category");
    String quid = server.arg("uid");
    String quserEmail = server.arg("email");
    String quserPass = server.arg("userpass");
    
    if (qsid.length() > 0 && qpass.length() > 0 && qdevice.length() > 0 && 
        qcategory.length() > 0 && quid.length() > 0 && quserEmail.length() > 0 && quserPass.length() > 0) {
      clearEEPROM();
      for (int i = 0; i < qsid.length(); ++i) {
        EEPROM.write(i, qsid[i]);
      }
      for (int i = 0; i < qpass.length(); ++i) {
        EEPROM.write(32 + i, qpass[i]);
      }
      for (int i = 0; i < qdevice.length(); ++i) {
        EEPROM.write(96 + i, qdevice[i]);
      }
      for (int i = 0; i < qcategory.length(); ++i) {
        EEPROM.write(160 + i, qcategory[i]);
      }
      for (int i = 0; i < quid.length(); ++i) {
        EEPROM.write(224 + i, quid[i]);
      }
      for (int i = 0; i < quserEmail.length(); ++i) {
        EEPROM.write(288 + i, quserEmail[i]);
      }
      for (int i = 0; i < quserPass.length(); ++i) {
        EEPROM.write(352 + i, quserPass[i]);
      }
      EEPROM.commit();
      digitalWrite(ledPin, HIGH);
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Saved to EEPROM. Restarting ESP32...\"}");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("Invalid input received");
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid input\"}");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void readEEPROM() {
  ssid = "";
  passphrase = "";
  deviceName = "";
  category = "";
  uid = "";
  userEmail = "";
  userPassword = "";
  
  for (int i = 0; i < 32 && EEPROM.read(i) != 0; ++i) {
    ssid += char(EEPROM.read(i));
  }
  for (int i = 32; i < 96 && EEPROM.read(i) != 0; ++i) {
    passphrase += char(EEPROM.read(i));
  }
  for (int i = 96; i < 160 && EEPROM.read(i) != 0; ++i) {
    deviceName += char(EEPROM.read(i));
  }
  for (int i = 160; i < 224 && EEPROM.read(i) != 0; ++i) {
    category += char(EEPROM.read(i));
  }
  for (int i = 224; i < 288 && EEPROM.read(i) != 0; ++i) {
    uid += char(EEPROM.read(i));
  }
  for (int i = 288; i < 352 && EEPROM.read(i) != 0; ++i) {
    userEmail += char(EEPROM.read(i));
  }
  for (int i = 352; i < 416 && EEPROM.read(i) != 0; ++i) {
    userPassword += char(EEPROM.read(i));
  }

  ssid.trim();
  passphrase.trim();
  deviceName.trim();
  category.trim();
  uid.trim();
  userEmail.trim();
  userPassword.trim();
  
  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + passphrase);
  Serial.println("DeviceName: " + deviceName);
  Serial.println("Category: " + category);
  Serial.println("UID: " + uid);
  Serial.println("Email: " + userEmail);
  Serial.println("User Pass length: " + String(userPassword.length()));
}

void clearEEPROM() {
  for (int i = 0; i < 512; ++i) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM cleared");
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  // Create a buffer to store the formatted time string
  char timeString[50];
  strftime(timeString, sizeof(timeString), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  
  // Print the formatted string
  Serial.println(timeString);
}