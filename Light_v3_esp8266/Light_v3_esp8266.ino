#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include <LittleFS.h>

// Light Control Pin Definitions
#define LED_PIN D7        // LED connected to GPIO 13 (D7)
#define RELAY_PIN D1      // Relay connected to GPIO 5 (D1)
#define BUTTON_PIN D2     // Push button connected to GPIO 4 (D2)
#define POT_PIN A0        // Potentiometer connected to Analog 0


// LED Pin Definitions
#define LED_RED D5    // Red LED for error/failure
#define LED_GREEN D6  // Green LED for success
#define LED_BLUE D3   // Blue LED for processing/waiting

// Button Pin Definition
#define CLEAR_EEPROM_BUTTON D0  // Button to clear EEPROM

// Button long press detection
unsigned long buttonPressStartTime = 0;
const unsigned long LONG_PRESS_TIME = 3000; // 3 seconds long press
// Firebase configuration
#define API_KEY "AIzaSyDqLcufSIP87WT1aEyDI-dKVdTsSBACrEc"
#define FIREBASE_PROJECT_ID "interactive-iot-devices-ssrip"

// Global variables for light control
int currentBrightness = 0;
int lastSentBrightness = -1;  // Track last brightness sent to Firestore
String currentDeviceStatus = "OFF";
String lastSentStatus = "";  // Track last status sent to Firestore

// Timing variables
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // 50ms for button debounce
const unsigned long updateDelay = 1000;  // 1 second delay between Firestore updates
unsigned long lastFirestoreUpdate = 0;
unsigned long lastFirestoreCheck = 0;
const unsigned long firestoreCheckInterval = 5000;  // Check Firestore every 5 seconds

// Button state variables
bool buttonPressed = false;
bool pendingUpdate = false;

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

// Establishing Local server at port 80
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(512);
  LittleFS.begin();
  delay(10);

  // Initialize LED and Reset Pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);



  
  // New LED and EEPROM button initialization
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(CLEAR_EEPROM_BUTTON, INPUT_PULLUP);

  // Initial LED states


  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);

  // Show startup status
  showInitStatus();

  // Restore last light state from LittleFS
  File stateFile = LittleFS.open("/light_state.txt", "r");
  if (stateFile) {
    currentBrightness = stateFile.parseInt();
    stateFile.close();
  }
  currentDeviceStatus = (currentBrightness == 0) ? "OFF" : "ON";
  lightBrightnessControl(currentBrightness);

  Serial.println("Startup");

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
    // Indicate WiFi connection attempt

    
    WiFi.begin(ssid.c_str(), passphrase.c_str());
    if (testWifi()) {
      Serial.println("Successfully Connected!!!");


      
      handleRequests();
      return;
    }
  }
  
  Serial.println("Turning the HotSpot On");
  
  setupAP();
  handleRequests();
}
void loop() {
  unsigned long currentMillis = millis();
  
  // Handle manual light control
  manualLightControl();
  
  

  // Check for EEPROM clear button
  checkEEPROMClearButton();
  
  // If there's a pending update, try to send it to Firestore
  if (pendingUpdate) {
    updateFirestoreState(currentBrightness, currentDeviceStatus);
  }

  server.handleClient(); // Handle client requests
}
void checkEEPROMClearButton() {
    // Check if button is pressed
    if (digitalRead(CLEAR_EEPROM_BUTTON) == LOW) {
        // Record the start time of the button press
        if (buttonPressStartTime == 0) {
            buttonPressStartTime = millis();
        }
        
        // Check if button is held for long press duration
        if (millis() - buttonPressStartTime >= LONG_PRESS_TIME) {
            // Clear EEPROM
            clearEEPROM();
            
            // Show clear status
            showClearEEPROMStatus();
            
            // Print to serial
            Serial.println("EEPROM Cleared!");
            
            // Reset button press start time
            buttonPressStartTime = 0;
            
            // Optional: Restart device
            delay(1000);
            ESP.restart();
        }
    } else {
        // Reset button press start time when button is released
        buttonPressStartTime = 0;
    }
}
// LED Status Patterns
void blinkLED(int pin, int times, int delayMs = 200) {
    for (int i = 0; i < times; i++) {
        digitalWrite(pin, HIGH);
        delay(delayMs);
        digitalWrite(pin, LOW);
        delay(delayMs);
    }
}

void showInitStatus() {
    // Startup pattern: Blue LED blinks slowly
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BLUE, HIGH);
        delay(500);
        digitalWrite(LED_BLUE, LOW);
        delay(500);
    }
}

void showSuccessStatus() {
    // Success pattern: Green LED blinks quickly
    blinkLED(LED_GREEN, 5, 100);
}

void showErrorStatus() {
    // Error pattern: Red LED blinks slowly
    blinkLED(LED_RED, 3, 500);
}

void showClearEEPROMStatus() {
    // Special status for EEPROM clear: Alternating Red and Blue
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_BLUE, LOW);
        delay(200);
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_BLUE, HIGH);
        delay(200);
    }
    digitalWrite(LED_BLUE, LOW);
}
void manualLightControl() {
  int potValue = analogRead(POT_PIN);
  int newBrightness = map(potValue, 0, 1023, 0, 100);
  bool stateChanged = false;
  
  // Check button for on/off toggle
  if (digitalRead(BUTTON_PIN) == LOW && !buttonPressed) {
    currentDeviceStatus = (currentDeviceStatus == "ON") ? "OFF" : "ON";
    buttonPressed = true;
    stateChanged = true;
    
    // If turning off, set brightness to 0
    if (currentDeviceStatus == "OFF") {
      currentBrightness = 0;

    } else {

    }

    delay(50); // Debounce delay
  } else if (digitalRead(BUTTON_PIN) == HIGH) {
    buttonPressed = false;
  }

  // If device is ON, adjust brightness
  if (currentDeviceStatus == "ON") {
    if (abs(newBrightness - currentBrightness) > 2) { // Add some hysteresis
      currentBrightness = newBrightness;
      stateChanged = true;
    }
  }

  // If state changed, update controls and mark for Firestore update
  if (stateChanged) {
    lightBrightnessControl(currentBrightness);
    
    // Save state using LittleFS
    File stateFile = LittleFS.open("/light_state.txt", "w");
    if (stateFile) {
      stateFile.println(currentBrightness);
      stateFile.close();
    }
    
    pendingUpdate = true;
  }
}

void lightBrightnessControl(int brightness) {
  if (brightness == 0) {
    digitalWrite(RELAY_PIN, LOW);
    analogWrite(LED_PIN, 1023); // Turn off LED

  } else {
    digitalWrite(RELAY_PIN, HIGH);
    // Invert brightness for PWM (1023 is off, 0 is full brightness)
    int pwmValue = 1023 - map(brightness, 0, 100, 0, 1023);
    analogWrite(LED_PIN, pwmValue);

  }
  
  // Debug print
  Serial.print("Brightness: ");
  Serial.print(brightness);
  Serial.println("%");
}

void checkDeviceStatus() {
  unsigned long currentTime = millis();
  
  // Only check Firestore if enough time has passed and no pending manual updates
  if (currentTime - lastFirestoreCheck >= firestoreCheckInterval && !pendingUpdate) {
    String documentPath = "Data/" + uid + "/" + category + "/" + deviceName;

    if (uid.length() == 0 || category.length() == 0 || deviceName.length() == 0) {
      return;
    }

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/" + documentPath;

    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);  // Updated begin method
    http.addHeader("Content-Type", "application/json");

    String idToken = getFirebaseToken();
    if (idToken.length() < 100) {
      return;
    }

    http.addHeader("Authorization", "Bearer " + idToken);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        String newDeviceStatus = doc["fields"]["deviceStatus"]["stringValue"].as<String>();
        int newBrightness = doc["fields"]["setBrightness"]["integerValue"].as<int>();
        
        if (newDeviceStatus != currentDeviceStatus || newBrightness != currentBrightness) {
          currentDeviceStatus = newDeviceStatus;
          currentBrightness = newBrightness;
          lightBrightnessControl(currentBrightness);
          
          // Remove preferences, use LittleFS instead
          File stateFile = LittleFS.open("/brightness_state.txt", "w");
          if (stateFile) {
            stateFile.println(currentBrightness);
            stateFile.close();
          }
          
          lastSentBrightness = currentBrightness;
          lastSentStatus = currentDeviceStatus;
        }
      }
    }

    http.end();
    lastFirestoreCheck = currentTime;
  }
}

void updateFirestoreState(int brightness, String status) {
  unsigned long currentTime = millis();
  
  // Only update if values have changed and enough time has passed
  if ((brightness != lastSentBrightness || status != lastSentStatus) && 
      (currentTime - lastFirestoreUpdate >= updateDelay)) {
    
    String documentPath = "Data/" + uid + "/" + category + "/" + deviceName;
    
    if (uid.length() == 0 || category.length() == 0 || deviceName.length() == 0) {
      return;
    }

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/" + documentPath;

    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);  // Updated begin method
    http.addHeader("Content-Type", "application/json");

    String idToken = getFirebaseToken();
    if (idToken.length() < 100) {
      return;
    }

    http.addHeader("Authorization", "Bearer " + idToken);

    String jsonPayload = "{\"fields\": {\"setBrightness\": {\"integerValue\": " + String(brightness) + 
                         "}, \"deviceStatus\": {\"stringValue\": \"" + status + "\"}}}";

    int httpResponseCode = http.PATCH(jsonPayload);
    
    if (httpResponseCode > 0) {
      lastSentBrightness = brightness;
      lastSentStatus = status;
      pendingUpdate = false;
    }

    http.end();
    lastFirestoreUpdate = currentTime;
  } else {
    pendingUpdate = true;  // Mark that we need to update when possible
  }
}

String getFirebaseToken() {
  HTTPClient http;
  
  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(API_KEY);
  
  WiFiClient client;
  http.begin(client, url);  // Updated begin method
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
  WiFi.softAP(("SSRIP_Light" + deviceName).c_str(), "");
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
    String qssid = server.arg("ssid");
    String qpass = server.arg("pass");
    String qdevice = server.arg("deviceName");
    String qcategory = server.arg("category");
    String quid = server.arg("uid");
    String quserEmail = server.arg("email");
    String quserPass = server.arg("userpass");
    
    if (qssid.length() > 0 && qpass.length() > 0 && qdevice.length() > 0 && 
        qcategory.length() > 0 && quid.length() > 0 && quserEmail.length() > 0 && quserPass.length() > 0) {
      
      // Clear EEPROM first
      clearEEPROM();

      // Write credentials to EEPROM using new function
      writeToEEPROM(0, qssid);
      writeToEEPROM(32, qpass);
      writeToEEPROM(96, qdevice);
      writeToEEPROM(160, qcategory);
      writeToEEPROM(224, quid);
      writeToEEPROM(288, quserEmail);
      writeToEEPROM(352, quserPass);

      EEPROM.commit();
      
      // Indicate successful save
      digitalWrite(LED_GREEN, HIGH);
      delay(1000);
      digitalWrite(LED_GREEN, LOW);
      
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Saved to EEPROM. Restarting ESP32...\"}");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("Invalid input received");
      
      // Indicate error
      digitalWrite(LED_RED, HIGH);
      delay(1000);
      digitalWrite(LED_RED, LOW);
      
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid input\"}");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void writeToEEPROM(int startAddress, String data) {
    for (int i = 0; i < data.length(); i++) {
        EEPROM.write(startAddress + i, data[i]);
    }
    EEPROM.write(startAddress + data.length(), '\0'); // Null-terminate string
}

void clearEEPROM() {
    for (int i = 0; i < 512; ++i) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    Serial.println("EEPROM cleared");
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

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  // Use strftime() to format time instead of direct println
  char timeString[50];
  strftime(timeString, sizeof(timeString), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  Serial.println(timeString);
}