#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <time.h>

// DHT22 configuration
#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

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
const int buttonPin = 4;
const int ledPin = 21;

// NTP Server for time synchronization
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;  // GMT+5:30 for IST
const int   daylightOffset_sec = 0;

// Establishing Local server at port 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(512);
  delay(10);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  Serial.println("Startup");

  dht.begin();

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

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(ledPin, LOW);
    
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
      sendToFirestore(temperature, humidity);
    }
  } else if (WiFi.getMode() == WIFI_AP) {
    blinkLED();
  } else {
    digitalWrite(ledPin, HIGH);
  }

  if (digitalRead(buttonPin) == LOW) {
    Serial.println("Button pressed! Clearing EEPROM...");
    clearEEPROM();
    ESP.restart();
  }

  server.handleClient();

  delay(60000); // Wait for 1 minute before next reading
}

void sendToFirestore(float temperature, float humidity) {
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


  HTTPClient http;

  http.begin(url);
  
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
  HTTPClient http;
  
  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(API_KEY);
  
  http.begin(url);
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
  WiFi.softAP(("SSRIP_Main" + deviceName).c_str(), "");
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

void blinkLED() {
  digitalWrite(ledPin, HIGH);
  delay(500);
  digitalWrite(ledPin, LOW);
  delay(500);
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}