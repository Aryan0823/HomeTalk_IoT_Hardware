#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// Variables
String ssid = "";
String passphrase = "";
String deviceName = "";
String category = "";
String uid = "";

// Static IP configuration
IPAddress local_IP(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// Button pin
const int buttonPin = 4;

// Function Declaration
bool testWifi(void);
void setupAP(void);
void handleRequests(void);
void clearEEPROM(void);
void readEEPROM(void);

// Establishing Local server at port 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(512);
  delay(10);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("Startup");

  readEEPROM();

  if (ssid.length() > 0 && passphrase.length() > 0) {
    WiFi.begin(ssid.c_str(), passphrase.c_str());
    if (testWifi()) {
      Serial.println("Successfully Connected!!!");
      digitalWrite(LED_BUILTIN, HIGH);  // LED on when connected
      handleRequests();
      return;
    }
  }
  
  Serial.println("Turning the HotSpot On");
  setupAP();
  handleRequests();
}

void loop() {
  if ((WiFi.status() == WL_CONNECTED)) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (digitalRead(buttonPin) == LOW) {
    Serial.println("Button pressed! Clearing EEPROM...");
    clearEEPROM();
    ESP.restart();
  }

  server.handleClient();
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
  WiFi.softAP(("SSRIP" + deviceName).c_str(), "");
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
    
    Serial.println("Received parameters:");
    Serial.println("SSID: " + qsid);
    Serial.println("Pass: " + qpass);
    Serial.println("Device: " + qdevice);
    Serial.println("Category: " + qcategory);
    Serial.println("UID: " + quid);
    
    if (qsid.length() > 0 && qpass.length() > 0 && qdevice.length() > 0 && qcategory.length() > 0 && quid.length() > 0) {
      for (int i = 0; i < 512; ++i) {
        EEPROM.write(i, 0);
      }
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
      EEPROM.commit();
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
  for (int i = 0; i < 32; ++i) {
    ssid += char(EEPROM.read(i));
  }
  for (int i = 32; i < 96; ++i) {
    passphrase += char(EEPROM.read(i));
  }
  for (int i = 96; i < 160; ++i) {
    deviceName += char(EEPROM.read(i));
  }
  for (int i = 160; i < 224; ++i) {
    category += char(EEPROM.read(i));
  }
  for (int i = 224; i < 288; ++i) {
    uid += char(EEPROM.read(i));
  }
  ssid.trim();
  passphrase.trim();
  deviceName.trim();
  category.trim();
  uid.trim();
  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + passphrase);
  Serial.println("DeviceName: " + deviceName);
  Serial.println("Category: " + category);
  Serial.println("UID: " + uid);
}

void clearEEPROM() {
  for (int i = 0; i < 512; ++i) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM cleared");
}