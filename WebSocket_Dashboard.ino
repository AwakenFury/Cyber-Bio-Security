#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// 1. Create a server object on port 80
AsyncWebServer server(80);

// 2. Define your HTML code as a raw string literal for easy multi-line coding
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Inspector Suite | Home</title>
<style>
  body { font-family: monospace; background: #000; color: #fff; text-align: center; padding: 50px 20px; }
  h1 { text-transform: uppercase; letter-spacing: 5px; margin-bottom: 10px; }
  p { color: #666; margin-bottom: 40px; }
  .menu-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; max-width: 1200px; margin: 0 auto; }
  .card { border: 1px solid #333; padding: 30px; text-decoration: none; color: #fff; transition: 0.3s; text-align: left; }
  .card:hover { border-color: #fff; background: #111; }
  .card h2 { margin: 0 0 10px 0; font-size: 1.1em; text-transform: uppercase; }
  .card p { font-size: 0.85em; line-height: 1.5; margin: 0; color: #888; }
  .status-dot { display: inline-block; width: 8px; height: 8px; background: #fff; margin-right: 10px; }
  footer { margin-top: 60px; font-size: 0.7em; color: #333; border-top: 1px solid #222; padding-top: 20px; }
</style>
</head>
<body>

<h1>INSPECTOR SUITE</h1>
<p>Integrated Passive Logging & Signal Analysis</p>

<div class="menu-grid">

  <a href="Super_Scan.html" class="card" style="border-color: #fff;">
    <h2><span class="status-dot"></span>Super Scan (All)</h2>
    <p>Master log capturing WiFi, BLE, and Classic BT in a single stream. Best for global environment auditing.</p>
  </a>

  <a href="Wi-Fi_Only.html" class="card">
    <h2><span class="status-dot"></span>WiFi Monitor</h2>
    <p>Filtered 2.4GHz access point logging. Focuses on BSSID and Channel mapping for WiFi packet analysis.</p>
  </a>

  <a href="BLE_BT_Both_Scan.html" class="card">
    <h2><span class="status-dot"></span>Dual Bluetooth</h2>
    <p>Separated BLE and Classic BT panels. Tracks batch counts and proximity for all Bluetooth devices.</p>
  </a>

  <a href="Inspector_Mode.html" class="card">
    <h2><span class="status-dot"></span>USB Inspector</h2>
    <p>Registry for USB serial devices. Detects VID/PID chipsets and assigns security trust ratings.</p>
  </a>

</div>

<footer>SYSTEM_STATUS: OPERATIONAL // WEB_SERIAL_ENABLED: TRUE</footer>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Starting AP test...");

  // Start Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("TEST_ASYNC", "12345678", 6);

  Serial.print("AP started. IP Address: ");
  Serial.println(WiFi.softAPIP()); // Usually 192.168.4.1

  // 3. Define the route for the root URL ("/")
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  // 4. Start the server
  server.begin();
}

void loop() {
  // Asynchronous servers handle requests in the background, 
  // so the loop stays empty unless you have other tasks.
}
