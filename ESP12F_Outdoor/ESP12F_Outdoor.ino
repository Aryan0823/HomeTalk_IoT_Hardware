#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT22 configuration
#define DHTPIN 2 // GPIO 2
#define DHTTYPE DHT22
DHTesp dht;

// BH1750 Configuration
#define BH1750_ADDRESS 0x23
#define CONTINUOUS_HIGH_RES_MODE 0x10

// Firebase configuration
#define API_KEY "AIzaSyDqLcufSIP87WT1aEyDI-dKVdTsSBACrEc"
#define FIREBASE_PROJECT_ID "interactive-iot-devices-ssrip"

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

// Button pin
const int buttonPin = 13;

// NTP Server for time synchronization
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // GMT+5:30 for IST
const int daylightOffset_sec = 0; // Establishing Local server at port 80

ESP8266WebServer server(80);

// Function Prototypes
void readEEPROM();
void configureTime();
void displayCredentials();
bool testWifi();

void printLocalTime();
void handleRequests();
void setupAP();
void clearEEPROM();
bool hasValidCredentials();
void sendToFirestore(float temperature, float humidity, float luxValue);
String getFirebaseToken();
void displaySensorReadings(float temp, float hum, float lux);
void displaySensorError();
void displayWiFiStatus(bool connected);

void setup() {
	Serial.begin(115200);
	WiFi.disconnect();
	Wire.begin(4, 5); // SDA on GPIO4, SCL on GPIO5

	// Initialize OLED Display
	if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
		Serial.println(F("SSD1306 allocation failed"));
		for (;;); // Don't proceed, loop forever
	}
	display.clearDisplay();
	display.setTextColor(WHITE);
	display.setTextSize(1);
	display.setCursor(0, 0);
	display.println("SSRIP Outdoor Device");
	display.display();

	EEPROM.begin(512);
	delay(10);
	pinMode(buttonPin, INPUT_PULLUP);
	dht.setup(DHTPIN, DHTesp::DHT22);

	Wire.beginTransmission(BH1750_ADDRESS);
	Wire.write(CONTINUOUS_HIGH_RES_MODE);
	Wire.endTransmission();

	readEEPROM();
	displayCredentials();

	if (ssid.length() > 0 && passphrase.length() > 0 && uid.length() > 0 && userEmail.length() > 0 && userPassword.length() > 0) {
		display.println("Connecting WiFi...");
		display.display();
		WiFi.begin(ssid.c_str(), passphrase.c_str());
		if (testWifi()) {
			setupLocalTime();
			printLocalTime();
			handleRequests();
			return;
		}
	}

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
		display.clearDisplay();
		display.setCursor(0, 0);
		display.println("EEPROM Clearing...");
		display.println("Device Restarting");
		display.display();
		Serial.println("Button pressed! Clearing EEPROM...");
		clearEEPROM();
		delay(2000);
		ESP.restart();
	}

	// Check and send data every minute if WiFi is connected and we have valid credentials
	if (WiFi.status() == WL_CONNECTED && hasValidCredentials()) {
		if (currentMillis - previousMillisDataCheck >= intervalDataCheck) {
			previousMillisDataCheck = currentMillis;
			TempAndHumidity lastValues = dht.getTempAndHumidity();
			float humidity = lastValues.humidity;
			float temperature = lastValues.temperature;

			// Read light intensity
			Wire.requestFrom(BH1750_ADDRESS, 2);
			float luxValue = 0;
			if (Wire.available() == 2) {
				unsigned int lightLevel = Wire.read() << 8 | Wire.read();
				luxValue = lightLevel / 1.2;
			}

			if (isnan(humidity) || isnan(temperature)) {
				Serial.println("Failed to read from DHT sensor!");
				displaySensorError();
			} else {
				Serial.printf("Temperature: %.2fB0C, Humidity: %.2f%%, Light: %.2f lux\n", temperature, humidity, luxValue);
				displaySensorReadings(temperature, humidity, luxValue);
				sendToFirestore(temperature, humidity, luxValue);
			}
		}
		displayWiFiStatus(true);
	} else {
		displayWiFiStatus(false);
	}

	server.handleClient();
}

void displayCredentials() {
	display.clearDisplay();
	display.setCursor(0, 0);
	display.println("Credentials:");
	display.println("SSID: " + (ssid.length() > 0 ? ssid.substring(0, 12) : "Not Set"));
	display.println("Device: " + deviceName);
	display.println("UID: " + (uid.length() > 0 ? uid.substring(0,10) + "..." : "Not Set"));
	display.display();
}

void displaySensorReadings(float temp, float hum, float lux) {
	display.clearDisplay();
	display.setCursor(0, 0);
	display.println("Sensor Readings:");
	display.print("Temp: ");
	display.print(temp);
	display.println(" C");
	display.print("Hum: ");
	display.print(hum);
	display.println(" %");
	display.print("Light: ");
	display.print(lux);
	display.println(" lux");
	display.display();
}

void displaySensorError() {
	display.clearDisplay();
	display.setCursor(0, 0);
	display.println("SENSOR ERROR!");
	display.println("Check DHT22");
	display.println("connection");
	display.display();
}

void displayWiFiStatus(bool connected) {
	display.clearDisplay();
	display.setCursor(0,0);
	display.println("WiFi Status:");
	display.println(connected ? "Connected" : "Disconnected");
	if (!connected) {
		display.println("Check Credentials");
	}
	display.display();
}

bool hasValidCredentials() {
	return (uid.length() > 0 && category.length() > 0 && deviceName.length() > 0);
}

void sendToFirestore(float temperature, float humidity, float luxValue) {
	if (!hasValidCredentials()) {
		Serial.println("Invalid credentials. Cannot send data.");
		return;
	}
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) {
		Serial.println("Failed to obtain time.");
		return;
	}
	char dateStr[11], timeStr[9];
	strftime(dateStr,sizeof(dateStr),"%Y-%m-%d",&timeinfo);
	strftime(timeStr,sizeof(timeStr),"%H:%M:%S",&timeinfo);

	String documentPath = "Data/" + uid + "/" + category + "/" + deviceName + "/" + String(dateStr);
	String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/" + documentPath + "?documentId=" + timeStr;

	WiFiClientSecure client;
	client.setInsecure(); // Replace in production

	HTTPClient http;
	http.begin(client,url);
	http.addHeader("Content-Type", "application/json");

	String idToken = getFirebaseToken();
	if (idToken.isEmpty()) {
		Serial.println("Invalid ID token.");
		return;
	}

	http.addHeader("Authorization", "Bearer " + idToken);

	String payload = "{\"fields\": {\"Date\": {\"stringValue\": \"" + String(dateStr) + "\"}, \"Time\": {\"stringValue\": \"" + String(timeStr) + "\"}, \"temperature \": {\"doubleValue\": " + String(temperature) + "}, \"humidity\": {\"doubleValue\": " + String(humidity) + "}, \"light\": {\"doubleValue\": " + String(luxValue) + "}}}";

	int httpResponseCode = http.POST(payload);
	if (httpResponseCode > 0) {
		Serial.println("Data sent successfully: " + http.getString());
	} else {
		Serial.println("Error sending data: " + String(httpResponseCode));
	}

	http.end();
}

String getFirebaseToken() {
	WiFiClientSecure client;
	client.setInsecure(); // Replace in production

	HTTPClient http;
	String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(API_KEY);

	http.begin(client,url);
	http.addHeader("Content-Type", "application/json");

	String payload = "{\"email\":\"" + userEmail + "\",\"password\":\"" + userPassword + "\",\"returnSecureToken\":true}";

	int httpResponseCode = http.POST(payload);

	if (httpResponseCode > 0) {
		String response = http.getString();
		DynamicJsonDocument doc(2048);
		auto error = deserializeJson(doc,response);

		if (error) {
			Serial.println("JSON Parsing Failed!");
			return "";
		}

		return doc["idToken"].as<String>();
	} else {
		Serial.println("Error during token fetch: " + String(httpResponseCode));
	}

	http.end();
	return "";
}

bool testWifi(void) {
	int c=0;
	Serial.println("Waiting for Wifi to connect...");
	while(c<20) {
		if(WiFi.status()==WL_CONNECTED) {
			Serial.println("WiFi Connected!");
			return true;
		}
		delay(500);
		c++;
		Serial.print(".");
	}
	Serial.println("WiFi connection failed.");
	return false;
}

void setupAP(void) {
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(local_IP,gateway,subnet);
	WiFi.softAP(("SSRIP_outdoor"+deviceName).c_str(),"");
	Serial.println("Setting up AP");
	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());
}

void handleRequests() {
	server.on("/setting", HTTP_GET,[&]() {
		server.sendHeader("Access-Control-Allow-Origin","*");
		String qssid=server.arg("ssid");
		String qpass=server.arg("pass");
		String qdevice=server.arg("deviceName");
		String qcategory=server.arg("category");
		String quid=server.arg("uid");
		String quserEmail=server.arg("email");
		String quserPass=server.arg("userpass");

		if(qssid.length()>0&&qpass.length()>0&&qdevice.length()>0&&qcategory.length()>0&&quid.length()>0&&quserEmail.length()>0&&quserPass.length()>0) {
			clearEEPROM();
			for(int i=0; i<qssid.length(); ++i) {
				EEPROM.write(i,qssid[i]);
			}
			for(int i=0; i<qpass.length(); ++i) {
				EEPROM.write(32+i,qpass[i]);
			}
			for(int i=0; i<qdevice.length(); ++i) {
				EEPROM.write(96+i,qdevice[i]);
			}
			for(int i=0; i<qcategory.length(); ++i) {
				EEPROM.write(160+i,qcategory[i]);
			}
			for(int i=0; i<quid.length(); ++i) {
				EEPROM.write(224+i,quid[i]);
			}
			for(int i=0; i<quserEmail.length(); ++i) {
				EEPROM.write(288+i,quserEmail[i]);
			}
			for(int i=0; i<quserPass.length(); ++i) {
				EEPROM.write(352+i,quserPass[i]);
			}
			EEPROM.commit();

			// Display success message on OLED
			display.clearDisplay();
			display.setCursor(0,0);
			display.println("Settings Saved!");
			display.println("Device Restarting...");
			display.display();
			server.send(200,"application/json","{\"success\":true,\"message\":\"Saved to EEPROM. Restarting ESP32...\"}");
			delay(1000);
			ESP.restart();
		}
		else {

			// Display error message on OLED
			display.clearDisplay();
			display.setCursor(0,0);
			display.println("ERROR!");
			display.println("Invalid Settings");
			display.display();
			Serial.println("Invalid input received");
			server.send(400,"application/json","{\"success\":false,\"message\":\"Invalid input\"}");
		}
	});
}

void readEEPROM() {
	ssid="";
	passphrase="";
	deviceName="";
	category="";
	uid="";
	userEmail="";
	userPassword="";
	for(int i=0; i<32&&EEPROM.read(i)!=0; ++i) {
		ssid+=char(EEPROM.read(i));
	}
	for(int i=32; i<96&&EEPROM.read(i)!=0; ++i) {
		passphrase+=char(EEPROM.read(i));
	}
	for(int i=96; i<160&&EEPROM.read(i)!=0; ++i) {
		deviceName+=char(EEPROM.read(i));
	}
	for(int i=160; i<224&&EEPROM.read(i)!=0; ++i) {
		category+=char(EEPROM.read(i));
	}
	for(int i=224; i<288&&EEPROM.read(i)!=0; ++i) {
		uid+=char(EEPROM.read(i));
	}
	for(int i=288; i<352&&EEPROM.read(i)!=0; ++i) {
		userEmail+=char(EEPROM.read(i));
	}
	for(int i=352; i<416&&EEPROM.read(i)!=0; ++i) {
		userPassword+=char(EEPROM.read(i));
	}
	ssid.trim();
	passphrase.trim();
	deviceName.trim();
	category.trim();
	uid.trim();
	userEmail.trim();
	userPassword.trim();

	Serial.println("SSID: "+ssid);
	Serial.println("PASS: "+passphrase);
	Serial.println("DeviceName: "+deviceName);
	Serial.println("Category: "+category);
	Serial.println("UID: "+uid);
	Serial.println("Email: "+userEmail);
	Serial.println("User Pass length: "+String(userPassword.length()));
}

void clearEEPROM() {
	for(int i=0; i<512; ++i) {
		EEPROM.write(i,0);
	}
	EEPROM.commit();
	Serial.println("EEPROM cleared");
}

void setupLocalTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void printLocalTime() {
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now,&timeinfo);
	char timeString[50];
	strftime(timeString,sizeof(timeString),"%A,%B %d %Y %H:%M:%S",&timeinfo);
	Serial.println(timeString);
}