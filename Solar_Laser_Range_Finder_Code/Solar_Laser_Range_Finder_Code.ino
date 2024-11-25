#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <VL53L0X.h>

// WiFi credentials
const char *ssid = "ARRIS-EEF5";
const char *password = "5G5344102059";

// Create instances
WebServer server(80);
VL53L0X sensor;

// Variables to store readings
int distance = 0;
float percentFull = 0;

// Hopper configuration (in millimeters)
const float FULL_LEVEL_MM = 76.2;    // 3 inches in mm
const float EMPTY_LEVEL_MM = 304.8;  // 12 inches in mm
const float HOPPER_RANGE_MM = EMPTY_LEVEL_MM - FULL_LEVEL_MM;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize I2C with custom pins
  Wire.begin(A4, A3);  // SDA = A4, SCL = A3
  
  Serial.println("\n=== Pellet Hopper Monitor ===");
  
  // Initialize sensor first
  Serial.println("1. Initializing sensor...");
  if (!sensor.init()) {
    Serial.println("Failed to detect and initialize sensor!");
    while (1) { delay(1000); }
  }
  Serial.println("   Sensor initialized successfully!");
  sensor.setTimeout(500);
  sensor.startContinuous();

  // Connect to WiFi
  Serial.printf("2. Connecting to WiFi: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  
  Serial.println("\nSystem Ready!");
  Serial.println("Visit http://192.168.0.18 in your browser");
}

void loop() {
  // Read sensor
  distance = sensor.readRangeContinuousMillimeters();
  
  if (!sensor.timeoutOccurred()) {
    // Calculate fill percentage
    if (distance <= FULL_LEVEL_MM) {
      percentFull = 100.0;
    } else if (distance >= EMPTY_LEVEL_MM) {
      percentFull = 0.0;
    } else {
      percentFull = (1 - (distance - FULL_LEVEL_MM) / HOPPER_RANGE_MM) * 100.0;
    }
    
    // Print readings every second
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 1000) {
      Serial.printf("Distance: %dmm (%.1fin) | Hopper: %.1f%%\n", 
                   distance, distance/25.4, percentFull);
      lastPrint = millis();
    }
  }
  
  server.handleClient();
  delay(10);
}

void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <title>Pellet Hopper Monitor</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            body { 
                font-family: Arial, sans-serif; 
                margin: 20px;
                background: #f0f0f0;
                text-align: center;
            }
            .container {
                max-width: 600px;
                margin: 0 auto;
                background: white;
                padding: 20px;
                border-radius: 10px;
                box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            }
            h1 { color: #333; }
            .readings {
                font-size: 24px;
                margin: 20px 0;
            }
            .hopper {
                width: 200px;
                height: 300px;
                margin: 20px auto;
                border: 3px solid #666;
                border-radius: 10px;
                position: relative;
                background: #f8f8f8;
            }
            .level {
                position: absolute;
                bottom: 0;
                width: 100%;
                background: #8b4513;
                transition: height 0.5s;
                border-radius: 0 0 7px 7px;
            }
            .status {
                font-size: 20px;
                font-weight: bold;
                margin: 10px 0;
            }
            .warning { 
                color: #ff4444; 
                animation: blink 1s infinite;
            }
            @keyframes blink {
                50% { opacity: 0.5; }
            }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>Pellet Hopper Monitor</h1>
            <div class="hopper">
                <div id="level" class="level" style="height: 0%"></div>
            </div>
            <div class="readings">
                <div>Level: <span id="percent">--</span>%</div>
                <div>Distance: <span id="distance">--</span> inches</div>
            </div>
            <div id="status" class="status"></div>
        </div>
        <script>
            function updateReadings() {
                fetch('/data')
                    .then(response => response.json())
                    .then(data => {
                        document.getElementById('percent').textContent = data.percent.toFixed(1);
                        document.getElementById('distance').textContent = (data.distance / 25.4).toFixed(1);
                        document.getElementById('level').style.height = data.percent + '%';
                        
                        const status = document.getElementById('status');
                        if (data.percent < 20) {
                            status.textContent = 'WARNING: PELLET LEVEL VERY LOW!';
                            status.className = 'status warning';
                        } else if (data.percent < 40) {
                            status.textContent = 'Notice: Pellet level getting low';
                            status.className = 'status';
                        } else {
                            status.textContent = 'Pellet level OK';
                            status.className = 'status';
                        }
                    })
                    .catch(error => console.error('Error:', error));
            }
            
            // Update every second
            setInterval(updateReadings, 1000);
            updateReadings();  // Initial update
        </script>
    </body>
    </html>
  )";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{\"distance\":" + String(distance) + 
                ",\"percent\":" + String(percentFull) + "}";
  server.send(200, "application/json", json);
}