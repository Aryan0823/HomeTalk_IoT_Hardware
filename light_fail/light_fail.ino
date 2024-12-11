#include <WiFi.h>
#include <WebServer.h>

#define triacPulse 34  // GPIO 4
#define ZVC 32        // GPIO 12

int Slider_Value = 0;
int dimming = 7200;  // Initialize to minimum brightness

// WiFi credentials
const char* ssid = "Aryan2.4g";
const char* password = "aryankrish";

WebServer server(80);

// HTML page with slider
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>AC Dimmer Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; margin: 20px; }
        .slider-container { margin: 20px; }
        .slider {
            width: 80%;
            height: 25px;
            margin: 10px;
        }
        .value-display {
            font-size: 20px;
            margin: 10px;
        }
    </style>
</head>
<body>
    <h2>AC Dimmer Control</h2>
    <div class="slider-container">
        <input type="range" min="0" max="100" value="0" class="slider" id="brightnessSlider">
        <div class="value-display">Brightness: <span id="brightnessValue">0</span>%</div>
    </div>
    <script>
        var slider = document.getElementById("brightnessSlider");
        var output = document.getElementById("brightnessValue");
        output.innerHTML = slider.value;
        
        slider.oninput = function() {
            output.innerHTML = this.value;
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/slider?value=" + this.value, true);
            xhr.send();
        }
        
        // Update slider value every 2 seconds
        setInterval(function() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (this.readyState == 4 && this.status == 200) {
                    var value = this.responseText;
                    slider.value = value;
                    output.innerHTML = value;
                }
            };
            xhr.open("GET", "/getValue", true);
            xhr.send();
        }, 2000);
    </script>
</body>
</html>
)rawliteral";

void setup() {
    pinMode(ZVC, INPUT_PULLUP);
    pinMode(triacPulse, OUTPUT);
    Serial.begin(9600);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Define web server routes
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", index_html);
    });

    server.on("/slider", HTTP_GET, []() {
        Slider_Value = server.arg("value").toInt();
        dimming = map(Slider_Value, 0, 100, 7200, 200);
        server.send(200, "text/plain", "OK");
    });

    server.on("/getValue", HTTP_GET, []() {
        server.send(200, "text/plain", String(Slider_Value));
    });

    server.begin();
    
    // Attach interrupt for zero-crossing detection
    attachInterrupt(digitalPinToInterrupt(ZVC), acon, FALLING);
}

void loop() {
    server.handleClient();
}

void acon() {
    delayMicroseconds(dimming);  // Delay based on slider value
    digitalWrite(triacPulse, HIGH);  // Turn on triac
    delayMicroseconds(50);  // Delay for output pulse
    digitalWrite(triacPulse, LOW);  // Turn off triac
}