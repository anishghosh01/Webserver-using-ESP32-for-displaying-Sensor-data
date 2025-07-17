#if !( defined(ESP32) )
  #error This code is designed for (ESP32 + ENC28J60) to run on ESP32 platform!
#endif

#define DEBUG_ETHERNET_WEBSERVER_PORT       Serial
#define _ETHERNET_WEBSERVER_LOGLEVEL_       3

#include <WebServer_ESP32_ENC.h>
#include <WebSockets2_Generic.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>

using namespace websockets2_generic;

// Enter a MAC address and IP address for your controller below.
#define NUMBER_OF_MAC      20

// Network configuration
byte mac[][NUMBER_OF_MAC] =
{
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x02 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x03 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x04 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x05 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x06 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x07 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x08 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x09 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0A },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0B },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0C },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0D },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0E },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0F },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x10 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x11 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x12 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x13 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x14 },
};

IPAddress myIP(192, 168, 0, 232);
IPAddress myGW(192, 168, 0, 1);
IPAddress mySN(255, 255, 255, 0);
IPAddress myDNS(8, 8, 8, 8);

// Sensor object
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// Web Server
WebServer server(80);

// WebSocket Server
WebsocketsServer wsServer;

// WebSocket client
WebsocketsClient wsClient;
bool clientConnected = false;

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 ENC28J60 BMP180 Monitor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }
        .header { text-align: center; margin-bottom: 30px; }
        .sensor-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; }
        .sensor-card { background: #fff; border-radius: 8px; padding: 15px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .sensor-value { font-size: 24px; font-weight: bold; margin: 10px 0; }
        .sensor-label { color: #666; }
        #temp-value { color: #e74c3c; }
        #pressure-value { color: #3498db; }
        #altitude-value { color: #2ecc71; }
        .timestamp { text-align: center; margin-top: 20px; color: #777; }
        #download-btn { 
            display: block; 
            margin: 20px auto; 
            padding: 10px 20px; 
            background: #3498db; 
            color: white; 
            border: none; 
            border-radius: 4px; 
            cursor: pointer;
        }
        #download-btn:hover { background: #2980b9; }
    </style>
</head>
<body>
    <div class="header">
        <h1>BMP180 Sensor Monitoring</h1>
        <p>ESP32 with ENC28J60 Ethernet</p>
    </div>
    
    <div class="sensor-grid">
        <div class="sensor-card">
            <div class="sensor-label">Temperature</div>
            <div class="sensor-value" id="temp-value">--</div>
            <div class="unit">°C</div>
        </div>
        
        <div class="sensor-card">
            <div class="sensor-label">Pressure</div>
            <div class="sensor-value" id="pressure-value">--</div>
            <div class="unit">hPa</div>
        </div>
        
        <div class="sensor-card">
            <div class="sensor-label">Altitude</div>
            <div class="sensor-value" id="altitude-value">--</div>
            <div class="unit">meters</div>
        </div>
    </div>
    
    <div class="timestamp" id="timestamp">Last update: --</div>
    
    <button id="download-btn" onclick="downloadCSV()">Download CSV Data</button>

    <script>
        var socket;
        var sensorData = [];
        var lastUpdate = 0;
        
        function initWebSocket() {
            socket = new WebSocket('ws://' + window.location.hostname + ':81/');
            
            socket.onmessage = function(event) {
                var now = Date.now();
                // Throttle updates to 200ms for smoother display
                if (now - lastUpdate >= 200) {
                    var data = JSON.parse(event.data);
                    updateDisplay(data);
                    
                    // Add timestamp and store data
                    data.timestamp = new Date().toISOString();
                    sensorData.push(data);
                    lastUpdate = now;
                }
            };
            
            socket.onclose = function() {
                setTimeout(initWebSocket, 1000);
            };
        }
        
        function updateDisplay(data) {
            document.getElementById('temp-value').textContent = data.temp.toFixed(2);
            document.getElementById('pressure-value').textContent = (data.pressure/100).toFixed(2);
            document.getElementById('altitude-value').textContent = data.altitude.toFixed(2);
            document.getElementById('timestamp').textContent = 'Last update: ' + new Date().toLocaleTimeString();
        }
        
        function downloadCSV() {
            if (sensorData.length === 0) {
                alert("No data to export!");
                return;
            }
            
            // CSV header
            let csv = "Timestamp,Temperature (°C),Pressure (hPa),Altitude (m)\n";
            
            // Add data rows
            sensorData.forEach(data => {
                csv += `${data.timestamp},${data.temp.toFixed(2)},${(data.pressure/100).toFixed(2)},${data.altitude.toFixed(2)}\n`;
            });
            
            // Create download link
            const blob = new Blob([csv], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.setAttribute('hidden', '');
            a.setAttribute('href', url);
            a.setAttribute('download', 'sensor_data.csv');
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
        }
        
        window.onload = function() {
            initWebSocket();
        };
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000);

    Serial.print(F("\nStart WebServer on "));
    Serial.print(ARDUINO_BOARD);
    Serial.print(F(" with "));
    Serial.println(SHIELD_TYPE);
    Serial.println(WEBSERVER_ESP32_ENC_VERSION);

    // Initialize BMP180 sensor
    if (!bmp.begin()) {
        Serial.println("Could not find BMP180 sensor!");
        while (1);
    }

    // Start Ethernet
    ESP32_ENC_onEvent();
    ETH.begin(MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST);
    ETH.config(myIP, myGW, mySN, myDNS);
    ESP32_ENC_waitForConnect();

    // Start Web Server
    server.on("/", handleRoot);
    server.begin();

    // Start WebSocket Server
    wsServer.listen(81);

    Serial.print("HTTP Server at: http://");
    Serial.println(ETH.localIP());
    Serial.print("WebSocket Server at: ws://");
    Serial.print(ETH.localIP());
    Serial.println(":81/");
}

void sendSensorData() {
    sensors_event_t event;
    bmp.getEvent(&event);

    ///float output_vol = analog.Write(6);
    
    float temperature;
    bmp.getTemperature(&temperature);
    float altitude = bmp.pressureToAltitude(1013.25, event.pressure);
    
    String jsonData = "{";
    jsonData += "\"temp\":" + String(temperature) + ",";
    jsonData += "\"pressure\":" + String(event.pressure) + ",";
    jsonData += "\"altitude\":" + String(altitude);
    //jsonData += "\"output voltage\":" + String(output_vol);
    jsonData += "}";
    
    if (clientConnected) {
        wsClient.send(jsonData);
    }
}

void loop() {
    server.handleClient();
    
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    // Check for new WebSocket client
    if (!clientConnected) {
        wsClient = wsServer.accept();
        if (wsClient.available()) {
            clientConnected = true;
            wsClient.onMessage([](WebsocketsMessage message) {
                // Handle incoming messages if needed
            });
        }
    }
    
    // Check if client is still connected
    if (clientConnected && !wsClient.available()) {
        clientConnected = false;
        wsClient.close();
    }
    
    // Send updates every 50ms (20 times per second)
    if (now - lastUpdate >= 50) {
        if (clientConnected) {
            sendSensorData();
            wsClient.poll();
        }
        lastUpdate = now;
    }
    
    // Small delay to prevent watchdog trigger
    delay(1);
}
