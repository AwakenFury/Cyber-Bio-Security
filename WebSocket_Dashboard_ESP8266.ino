#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFiUdp.h>

extern "C" {
  #include "user_interface.h"
}

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================
// OLED CONFIG
// ======================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ======================
// PINS
// ======================

#define EMG_PIN A0

// ======================
// THRESHOLDS
// ======================

#define EMG_THRESHOLD 500
#define OLED_UPDATE_INTERVAL 100

// ======================
// VARIABLES
// ======================

float wifiSmoothedRssi = NAN;
float emgSmoothed = NAN;

unsigned long lastOLEDUpdate = 0;

// ======================
// AP STATE TRACKING
// ======================

uint8_t apStationCount = 0;

bool apConnected = false;

bool connectEvent = false;
bool disconnectEvent = false;

unsigned long eventTimer = 0;

// ======================
// UDP
// ======================

WiFiUDP udp;
const int udpPort = 4210;
IPAddress broadcastIp(255,255,255,255);

// ======================
// WEB SERVER
// ======================

AsyncWebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;

// ======================
// HTML PAGE
// ======================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Inspector Suite</title>

<style>
body{
  background:#000;
  color:#fff;
  font-family:monospace;
  text-align:center;
  padding-top:40px;
}
.card{
  border:1px solid #444;
  padding:20px;
  margin:20px auto;
  width:300px;
}
.value{
  font-size:2em;
  margin-top:10px;
}
</style>
</head>

<body>

<h1>INSPECTOR SUITE</h1>

<div class="card">
  <h2>WiFi RSSI</h2>
  <div class="value" id="wifi">0</div>
</div>

<div class="card">
  <h2>EMG</h2>
  <div class="value" id="emg">0</div>
</div>

<div class="card">
  <h2>Stations</h2>
  <div class="value" id="sta">0</div>
</div>

<script>

setInterval(() => {

fetch("/status")
.then(r => r.json())
.then(data => {

  document.getElementById("wifi").innerText = data.wifi;
  document.getElementById("emg").innerText = data.emg;
  document.getElementById("sta").innerText = data.stations;

});

}, 500);

</script>

</body>
</html>
)rawliteral";

// ======================
// WIFI SNIFFER CALLBACK
// ======================

void ICACHE_FLASH_ATTR wifi_sniffer(uint8_t *buf, uint16_t len) {

  signed int rssi = buf[0];

  if(isnan(wifiSmoothedRssi))
    wifiSmoothedRssi = rssi;
  else
    wifiSmoothedRssi =
      wifiSmoothedRssi * 0.85 + rssi * 0.15;
}

// ======================
// SETUP
// ======================

void setup() {

  Serial.begin(115200);

  // ======================
  // OLED INIT
  // ======================

  Wire.begin(12,14);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Failed");
    while(true);
  }

  display.clearDisplay();
  display.display();

  // ======================
  // AP MODE
  // ======================

  WiFi.mode(WIFI_AP);

  IPAddress local_ip(192,168,10,1);
  IPAddress gateway(192,168,10,1);
  IPAddress subnet(255,255,255,0);

  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP("InspectorSuite", "12345678");

  Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", local_ip);

  // ======================
  // PROMISCUOUS MODE
  // ======================

  wifi_set_opmode(STATIONAP_MODE);

  wifi_set_promiscuous_rx_cb(wifi_sniffer);

  wifi_promiscuous_enable(1);

  // ======================
  // UDP
  // ======================

  udp.begin(udpPort);

  // ======================
  // WEB ROUTES
  // ======================

  server.on("/", HTTP_GET,
  [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/status", HTTP_GET,
  [](AsyncWebServerRequest *request) {

    String json = "{";

    json += "\"wifi\":";
    json += String(wifiSmoothedRssi,1);
    json += ",";

    json += "\"emg\":";
    json += String(emgSmoothed,1);
    json += ",";

    json += "\"stations\":";
    json += String(apStationCount);

    json += "}";

    request->send(200, "application/json", json);
  });

  server.on("/generate_204", HTTP_GET,
  [](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  server.on("/hotspot-detect.html", HTTP_GET,
  [](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  server.onNotFound(
  [](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  server.begin();

  Serial.println("SERVER STARTED");
}

// ======================
// LOOP
// ======================

void loop() {

  dnsServer.processNextRequest();

  unsigned long currentTime = millis();

  // ======================
  // UDP PING
  // ======================

  static unsigned long lastPing = 0;

  if(currentTime - lastPing >= 100) {

    lastPing = currentTime;

    udp.beginPacket(broadcastIp, udpPort);
    udp.write("PING");
    udp.endPacket();
  }

  // ======================
  // EMG
  // ======================

  int emgValue = analogRead(EMG_PIN);

  if(isnan(emgSmoothed))
    emgSmoothed = emgValue;
  else
    emgSmoothed =
      emgSmoothed * 0.9 +
      emgValue * 0.1;

  // ======================
  // AP STATE LOGIC
  // ======================

  apStationCount = wifi_softap_get_station_num();

  bool newState = (apStationCount > 0);

  if(newState != apConnected) {

    if(newState) {
      connectEvent = true;
      disconnectEvent = false;
    } else {
      connectEvent = false;
      disconnectEvent = true;
    }

    eventTimer = currentTime;
    apConnected = newState;
  }

  if(currentTime - eventTimer > 2000) {
    connectEvent = false;
    disconnectEvent = false;
  }

  // ======================
  // OLED UPDATE
  // ======================

  if(currentTime - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {

    lastOLEDUpdate = currentTime;

    display.clearDisplay();

    // STATUS LINE
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);

    display.print("STATUS: ");

    if(connectEvent) {
      display.print("CONNECT");
    }
    else if(disconnectEvent) {
      display.print("DISCONNECT");
    }
    else {
      display.print(apConnected ? "CONNECTED" : "OK");
    }

    // MAIN DATA
    display.setCursor(0,10);

    display.print("IP: ");
    display.println(WiFi.softAPIP());

    display.print("STA: ");
    display.println(apStationCount);

    display.print("EMG: ");
    display.println(emgSmoothed, 1);

    display.print("WiFi: ");
    display.println(wifiSmoothedRssi, 1);

    display.display();
  }

  delay(10);
}
