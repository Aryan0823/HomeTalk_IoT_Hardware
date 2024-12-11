#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
#define WIFI_SSID "Aryan2.4g"
#define WIFI_PASSWORD "aryankrish"

// Firebase project credentials
#define API_KEY "AIzaSyD98TB7ddAuWW6fs6-ssa3H5Oqq8KPLLVU"
#define USER_EMAIL "aryanthakore21@gnu.ac.in" // Your email
#define USER_PASSWORD "aryankrish"             // Your password

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected to WiFi");
}

void setup() {
  Serial.begin(115200);
  
  initWiFi();

  // Prepare the HTTP request
  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(API_KEY);
  String payload = "{\"email\":\"" + String(USER_EMAIL) + "\",\"password\":\"" + String(USER_PASSWORD) + "\",\"returnSecureToken\":true}";

  // Make the HTTP POST request
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);

    // Parse the response to extract the user UID
    if (response.indexOf("idToken") > 0) {
      int uidStart = response.indexOf("localId") + 10; // Adjust index to get the UID
      int uidEnd = response.indexOf("\"", uidStart);
      String uid = response.substring(uidStart, uidEnd);
      Serial.print("User UID: ");
      Serial.println(uid);
    } else {
      Serial.println("Login failed: " + response);
    }
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end(); // Free resources
}

void loop() {
  // Keep the connection alive
  delay(1000);
}
