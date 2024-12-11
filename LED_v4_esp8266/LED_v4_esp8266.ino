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

// Configuration Constants
const unsigned long LONG_PRESS_TIME = 3000; // 3 seconds long press
const unsigned long DEBOUNCE_DELAY = 50;    // 50ms for button debounce
const unsigned long UPDATE_DELAY = 1000;    // 1 second delay between Firestore updates
const unsigned long FIRESTORE_CHECK_INTERVAL = 5000;  // Check Firestore every 5 seconds
const int WIFI_CONNECT_TIMEOUT = 20000;     // 20 seconds WiFi connection timeout
const int MAX_WIFI_ATTEMPTS = 3;            // Number of WiFi connection attempts

// Firebase configuration (consider moving sensitive data to a secure config file)
#define API_KEY "AIzaSyDqLcufSIP87WT1aEyDI-dKVdTsSBACrEc"
#define FIREBASE_PROJECT_ID "interactive-iot-devices-ssrip"

// Global variables for light control
struct DeviceState {
    int brightness = 0;
    String status = "OFF";
    int lastSentBrightness = -1;
    String lastSentStatus = "";
};

// Global objects and variables
DeviceState deviceState;
ESP8266WebServer server(80);
WiFiClient wifiClient;

// Timing and state tracking
struct TimeTracking {
    unsigned long buttonPressStartTime = 0;
    unsigned long lastDebounceTime = 0;
    unsigned long lastFirestoreUpdate = 0;
    unsigned long lastFirestoreCheck = 0;
    bool buttonPressed = false;
    bool pendingUpdate = false;
};
TimeTracking timeTrack;

// Credential storage
struct DeviceCredentials {
    String ssid;
    String passphrase;
    String deviceName;
    String category;
    String uid;
    String userEmail;
    String userPassword;
};
DeviceCredentials credentials;

// Static IP configuration
IPAddress local_IP(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
    // Enhanced initialization with more robust setup
    initializeHardware();
    
    // Restore previous state
    restorePreviousState();
    
    // Print debug information
    printDeviceInfo();
    
    // Attempt WiFi connection
    if (connectToWiFi()) {
        handleRequests();
    } else {
        setupAccessPoint();
        handleRequests();
    }
}
void setupAP(void) {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(("SSRIP_Light" + deviceName).c_str(), "");
  Serial.println("Setting up AP");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}
void initializeHardware() {
    Serial.begin(115200);
    while (!Serial) { ; } // Wait for serial port to connect
    
    // Initialize file system and EEPROM
    EEPROM.begin(512);
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed. Formatting...");
        LittleFS.format();
        LittleFS.begin();
    }
    
    // Pin mode setup with error checking
    pinMode(LED_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(CLEAR_EEPROM_BUTTON, INPUT_PULLUP);
    
    // Initial LED states
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    
    showInitStatus();
}

void restorePreviousState() {
    // Restore device state from LittleFS
    File stateFile = LittleFS.open("/device_state.json", "r");
    if (stateFile) {
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, stateFile);
        
        if (!error) {
            deviceState.brightness = doc["brightness"] | 0;
            deviceState.status = doc["status"].as<String>() | "OFF";
        }
        
        stateFile.close();
    }
    
    // Apply restored state
    lightBrightnessControl(deviceState.brightness);
}

void printDeviceInfo() {
    // Read credentials
    readEEPROM();
    
    // Debug print
    Serial.println("Device Credentials:");
    Serial.println("SSID: " + credentials.ssid);
    Serial.println("Device Name: " + credentials.deviceName);
    Serial.println("Category: " + credentials.category);
}

bool connectToWiFi() {
    // Check if credentials are valid
    if (credentials.ssid.isEmpty() || credentials.passphrase.isEmpty()) {
        return false;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(credentials.ssid.c_str(), credentials.passphrase.c_str());
    
    unsigned long startAttemptTime = millis();
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        
        if (millis() - startAttemptTime > WIFI_CONNECT_TIMEOUT) {
            Serial.println("\nWiFi connection failed");
            return false;
        }
    }
    
    Serial.println("\nWiFi Connected");
    showSuccessStatus();
    return true;
}

void setupAccessPoint() {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(("SSRIP_Light_" + credentials.deviceName).c_str(), "");
    
    Serial.println("Access Point Started");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void loop() {
    // Handle manual light control
    manualLightControl();
    
    // Check for EEPROM clear button
    checkEEPROMClearButton();
    
    // Update Firestore if needed
    if (timeTrack.pendingUpdate) {
        updateFirestoreState(deviceState.brightness, deviceState.status);
    }
    
    // Periodic Firestore check
    checkDeviceStatus();
    
    // Handle web server requests
    server.handleClient();
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

void saveDeviceState() {
    File stateFile = LittleFS.open("/device_state.json", "w");
    if (stateFile) {
        DynamicJsonDocument doc(256);
        doc["brightness"] = deviceState.brightness;
        doc["status"] = deviceState.status;
        
        serializeJson(doc, stateFile);
        stateFile.close();
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
void checkDeviceStatus() {
    unsigned long currentTime = millis();
    
    // Only check Firestore if enough time has passed and no pending manual updates
    if (currentTime - timeTrack.lastFirestoreCheck >= FIRESTORE_CHECK_INTERVAL && 
        !timeTrack.pendingUpdate) {
        
        // Validate required credentials for Firestore path
        if (credentials.uid.isEmpty() || 
            credentials.category.isEmpty() || 
            credentials.deviceName.isEmpty()) {
            Serial.println("Insufficient credentials for Firestore sync");
            return;
        }

        // Construct Firestore document path
        String documentPath = "Data/" + credentials.uid + "/" + 
                               credentials.category + "/" + credentials.deviceName;

        String url = "https://firestore.googleapis.com/v1/projects/" 
                     FIREBASE_PROJECT_ID "/databases/(default)/documents/" + documentPath;

        // Retrieve Firebase authentication token
        String idToken = getSecureFirebaseToken();
        if (idToken.length() < 100) {
            Serial.println("Failed to obtain Firebase token");
            return;
        }

        // Prepare HTTP client
        HTTPClient http;
        WiFiClient client;
        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", "Bearer " + idToken);

        // Perform GET request
        int httpResponseCode = http.GET();
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            
            // JSON parsing with error handling
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                // Safely extract device status and brightness
                JsonObject fields = doc["fields"];
                
                // Use default values if fields are missing
                String newDeviceStatus = fields.containsKey("deviceStatus") ? 
                    fields["deviceStatus"]["stringValue"].as<String>() : 
                    deviceState.status;
                
                int newBrightness = fields.containsKey("setBrightness") ? 
                    fields["setBrightness"]["integerValue"].as<int>() : 
                    deviceState.brightness;
                
                // Check if state has actually changed
                bool stateChanged = (newDeviceStatus != deviceState.status || 
                                     newBrightness != deviceState.brightness);
                
                if (stateChanged) {
                    // Update local state
                    deviceState.status = newDeviceStatus;
                    deviceState.brightness = newBrightness;
                    
                    // Apply new state
                    lightBrightnessControl(deviceState.brightness);
                    
                    // Save updated state
                    saveDeviceState();
                    
                    // Log state change
                    Serial.println("Device state updated from Firestore:");
                    Serial.print("Status: "); Serial.println(newDeviceStatus);
                    Serial.print("Brightness: "); Serial.println(newBrightness);
                }
            } else {
                Serial.print("JSON parsing failed: ");
                Serial.println(error.c_str());
            }
        } else {
            Serial.print("Firestore sync failed. Error code: ");
            Serial.println(httpResponseCode);
        }

        // Close HTTP connection
        http.end();
        
        // Update last check time
        timeTrack.lastFirestoreCheck = currentTime;
    }
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
void updateFirestoreState(int brightness, String status) {
    unsigned long currentTime = millis();
    
    if ((brightness != deviceState.lastSentBrightness || status != deviceState.lastSentStatus) && 
        (currentTime - timeTrack.lastFirestoreUpdate >= UPDATE_DELAY)) {
        
        String documentPath = "Data/" + credentials.uid + "/" + 
                               credentials.category + "/" + credentials.deviceName;
        
        if (credentials.uid.isEmpty() || credentials.category.isEmpty() || 
            credentials.deviceName.isEmpty()) {
            return;
        }

        String url = "https://firestore.googleapis.com/v1/projects/" 
                     FIREBASE_PROJECT_ID "/databases/(default)/documents/" + documentPath;

        HTTPClient http;
        WiFiClient client;
        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");

        String idToken = getSecureFirebaseToken();
        if (idToken.length() < 100) {
            return;
        }

        http.addHeader("Authorization", "Bearer " + idToken);

        String jsonPayload = "{\"fields\": {\"setBrightness\": {\"integerValue\": " + String(brightness) + 
                             "}, \"deviceStatus\": {\"stringValue\": \"" + status + "\"}}}";

        int httpResponseCode = http.PATCH(jsonPayload);
        
        if (httpResponseCode > 0) {
            deviceState.lastSentBrightness = brightness;
            deviceState.lastSentStatus = status;
            timeTrack.pendingUpdate = false;
        }

        http.end();
        timeTrack.lastFirestoreUpdate = currentTime;
    } else {
        timeTrack.pendingUpdate = true;
    }
}
String getSecureFirebaseToken() {
    // Enhanced token retrieval with caching and error handling
    static String cachedToken = "";
    static unsigned long tokenExpireTime = 0;
    
    unsigned long currentTime = millis();
    
    // Return cached token if it's still valid
    if (!cachedToken.isEmpty() && currentTime < tokenExpireTime) {
        return cachedToken;
    }

    // Prepare authentication request
    HTTPClient http;
    String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + 
                 String(API_KEY);
    
    WiFiClient client;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    // Construct authentication payload
    String jsonPayload = "{\"email\":\"" + credentials.userEmail + 
                         "\",\"password\":\"" + credentials.userPassword + 
                         "\",\"returnSecureToken\":true}";
    
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        
        // Parse JSON response
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            String idToken = doc["idToken"].as<String>();
            
            if (idToken.length() > 0) {
                // Cache token and set expiration (Firebase tokens typically last 1 hour)
                cachedToken = idToken;
                tokenExpireTime = currentTime + (60 * 60 * 1000); // 1 hour in milliseconds
                
                return idToken;
            } else {
                Serial.println("No valid token in response");
            }
        } else {
            Serial.print("Token parsing error: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.print("Token retrieval failed. Error code: ");
        Serial.println(httpResponseCode);
    }
    
    http.end();
    return ""; // Return empty string if token retrieval fails
}
// Optional: Add OTA (Over-The-Air) update capability
void setupOTA() {
    // Implementation for OTA updates would go here
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