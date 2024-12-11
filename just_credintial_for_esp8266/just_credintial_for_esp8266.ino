#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variables to store credentials
String ssid = "";
String passphrase = "";
String deviceName = "outdoor";
String category = "OutdoorSensors";
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
    EEPROM.begin(512); // Initialize EEPROM

    // Initialize OLED Display
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SSRIP Outdoor Device");
    display.display();

    // Set up access point with specified IP
    WiFi.softAPConfig(local_IP, gateway, subnet); // Configure AP with specific IP
    WiFi.softAP("SSRIP_outdoor", ""); // No password for simplicity

    Serial.println("Access Point started");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Set up server routes
    server.on("/setting", HTTP_GET, handleRequests);
    server.begin();
}

void loop() {
    server.handleClient(); // Handle incoming client requests
}

void handleRequests() {
    ssid = server.arg("ssid");
    passphrase = server.arg("pass");
    deviceName = server.arg("deviceName");
    category = server.arg("category");
    uid = server.arg("uid");
    userEmail = server.arg("email");
    userPassword = server.arg("userpass");

    Serial.println("Received request:");
    Serial.printf("SSID: %s\n", ssid.c_str());
    Serial.printf("Password: %s\n", passphrase.c_str());

    if (ssid.length() > 0 && passphrase.length() > 0) {
        clearEEPROM(); // Clear previous data

        writeToEEPROM(0, ssid); // Write SSID to EEPROM
        writeToEEPROM(32, passphrase); // Write Password to EEPROM
        writeToEEPROM(96, deviceName); // Write Device Name to EEPROM
        writeToEEPROM(160, category); // Write Category to EEPROM
        writeToEEPROM(224, uid); // Write UID to EEPROM
        writeToEEPROM(288, userEmail); // Write User Email to EEPROM
        writeToEEPROM(352, userPassword); // Write User Password to EEPROM

        EEPROM.commit(); // Commit changes to EEPROM

        Serial.println("Settings saved!");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Settings Saved!");
        display.display();

        delay(1000); // Delay before showing stored data
        readEEPROM(); // Read and display stored data on Serial and OLED

        server.send(200, "application/json", "{\"success\":true,\"message\":\"Settings saved\"}");
        
        delay(1000); 
        ESP.restart(); // Restart ESP after saving settings
    } else {
        Serial.println("Invalid input received");
        server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid input\"}");
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
}

void readEEPROM() {
   ssid = ""; passphrase = ""; deviceName = ""; category = ""; uid = ""; userEmail = ""; userPassword = "";

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

   Serial.println("Stored Credentials:");
   Serial.println("SSID: " + ssid);
   Serial.println("PASS: " + passphrase);
   Serial.println("Device Name: " + deviceName);
   Serial.println("Category: " + category);
   Serial.println("UID: " + uid);
   Serial.println("Email: " + userEmail);
}
