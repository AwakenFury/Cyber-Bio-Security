
#include <stdint.h>
#include <math.h>

#include "soc/spi_reg.h"
#include "driver/spi_master.h"

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

#define MAX_STA_DEVICES 10

struct STADevice {
  String mac;
  bool active;
};

STADevice staDevices[MAX_STA_DEVICES];
int staDeviceCount = 0;


#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiUdp.h>

// ======================================================
// WIFI AP
// ======================================================

const char* AP_NAME = "InspectorSuite";
const char* AP_PASS = "12345678";

// ======================================================
// WEB SERVER
// ======================================================

WebServer server(80);
DNSServer dnsServer;
WiFiUDP udp;

const byte DNS_PORT = 53;
const int udpPort = 4210;

IPAddress local_ip(192,168,10,1);
IPAddress gateway(192,168,10,1);
IPAddress subnet(255,255,255,0);
IPAddress broadcastIp(255,255,255,255);

// ======================================================
// LCD PINS
// ======================================================

#define LCD_CS    15
#define LCD_RST   4
#define LCD_DC    2
#define LCD_BL    27

#define SPI_MISO  12
#define SPI_MOSI  13
#define SPI_SCLK  14

// ======================================================
// SENSOR
// ======================================================

#define EMG_PIN 34

// ======================================================
// THRESHOLDS
// ======================================================

#define EMG_THRESHOLD      3500
#define MOTION_THRESHOLD   10
#define MOTION_HOLD        200
#define MAX_PULSES         50

// ======================================================
// SPI REGISTER ACCESS
// ======================================================

#define SPI_PORT 2

volatile uint32_t* spi_write_len =
  (volatile uint32_t*)(SPI_MOSI_DLEN_REG(SPI_PORT));

volatile uint32_t* spi_write_buf =
  (volatile uint32_t*)(SPI_W0_REG(SPI_PORT));

volatile uint32_t* spi_cmd =
  (volatile uint32_t*)(SPI_CMD_REG(SPI_PORT));

volatile uint32_t* spi_usr =
  (volatile uint32_t*)(SPI_USER_REG(SPI_PORT));

// ======================================================
// FAST GPIO
// ======================================================

#define DC_L GPIO.out_w1tc = (1 << LCD_DC)
#define DC_H GPIO.out_w1ts = (1 << LCD_DC)

#define CS_L GPIO.out_w1tc = (1 << LCD_CS)
#define CS_H GPIO.out_w1ts = (1 << LCD_CS)

// ======================================================
// COLORS
// ======================================================

#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREEN 0x07E0
#define RED   0xF800
#define BLUE  0x001F

// ======================================================
// VARIABLES
// ======================================================

float wifiRssi = -70;
float emgVal = 0;

bool motionActive = false;
bool emgActive = false;

String netStatus = "BOOT";
String ipAddress = "0.0.0.0";
int staCount = 0;

volatile unsigned long wifiPulseTimes[MAX_PULSES];
volatile int wifiPulseIndex = 0;

// ======================================================
// HTML PAGE
// ======================================================

const char index_html[] PROGMEM = R"rawliteral(





<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>USB Device Inspector</title>

<style>
/* --- SCREEN STYLES (Monochrome UI) --- */
body {
  font-family: monospace; /* Matching the hacker/inspector look */
  background: #000000;
  color: #ffffff;
  margin: 0;
  padding: 20px;
}

.grid {
  display: grid;
  grid-template-columns: 1fr 2fr;
  gap: 15px;
}

.panel {
  background: transparent;
  padding: 15px;
  border: 1px solid #333;
}

h2, h3 {
  margin-top: 0;
  text-transform: uppercase;
  letter-spacing: 2px;
}

h3 {
  color: #fff;
  border-bottom: 1px solid #333;
  padding-bottom: 5px;
}

button {
  padding: 10px 14px;
  margin: 5px;
  border: 1px solid #fff;
  border-radius: 0px;
  cursor: pointer;
  background: transparent;
  color: #fff;
  font-family: monospace;
  transition: 0.2s;
}

button:hover {
  background: #fff;
  color: #000;
}

#disconnect { border-color: #666; color: #666; }
#disconnect:hover { background: #666; color: #000; }

.box, .logbox {
  background: transparent;
  padding: 10px;
  border: 1px solid #333;
  margin-top: 10px;
}

.logbox {
  height: 260px;
  overflow: auto;
  font-size: 13px;
  line-height: 1.5;
  border-color: #222;
  color: #888;
}

.status {
  padding: 8px;
  border: 1px solid #fff;
  text-align: center;
  background: transparent;
  text-transform: uppercase;
  font-weight: bold;
}

.on { background: #fff !important; color: #000 !important; }
.off { border-style: dashed; color: #666; border-color: #666; }

.row {
  padding: 8px;
  border-bottom: 1px solid #222;
}

/* --- PRINT STYLES --- */
@media print {
  body { background: #fff !important; color: #000 !important; }
  .panel, .box, .logbox { border: 1px solid #000 !important; }
  button { display: none !important; }
  h3 { border-bottom: 2px solid #000 !important; color: #000 !important; }
  .status.on { background: #000 !important; color: #fff !important; }
  .logbox { color: #000 !important; height: auto !important; overflow: visible !important; }
}
</style>
</head>

<body>

<h2>🔍 Inspector Mode Upgrade</h2>

<div class="grid">

<!-- LEFT -->
<div class="panel">

<h3>Status</h3>
<div id="status" class="status off">Disconnected</div>

<h3>Controls</h3>
<button id="connect" onclick="connectPort()">Connect Device</button>
<button id="disconnect" onclick="disconnectPort()">Disconnect</button>
<button id="export" onclick="exportCSV()">Export CSV</button>

<h3>Current Device</h3>
<div id="deviceBox" class="box">
No device connected
</div>

<h3>Trust Rating</h3>
<div id="trustBox" class="box">
--
</div>

</div>

<!-- RIGHT -->
<div class="panel">

<h3>Known Devices Seen</h3>
<div id="historyBox" class="box">
None
</div>

<h3 style="margin-top:20px;">Inspector Logs</h3>
<div id="logBox" class="logbox"></div>

</div>

</div>

<script>
let port = null;
let devices = [];
let logs = [];

let knownSTA = {};


async function pollESP32(){

  try{

    let res = await fetch("/status");

    let data = await res.json();

    if(data.devices){

      data.devices.forEach(dev => {

        if(!knownSTA[dev.mac]){

          knownSTA[dev.mac] = true;

          let record = {
            timestamp: new Date().toLocaleString(),
            vid: "WIFI",
            pid: "STA",
            chip: "WiFi Client",
            trust: "NETWORK DEVICE",
            mac: dev.mac
          };

          devices.push(record);

          renderHistory();

          addLog("STA Connected: " + dev.mac);
        }
      });
    }

  }catch(e){

    addLog("ESP32 status offline");
  }
}

setInterval(pollESP32, 2000);



function addLog(msg){
  let ts = new Date().toLocaleTimeString();
  logs.push({ timestamp: ts, message: msg });
  let box = document.getElementById("logBox");
  box.innerHTML += "[" + ts + "] " + msg + "<br>";
  box.scrollTop = box.scrollHeight;
}

function setStatus(on){
  let el = document.getElementById("status");
  el.className = "status " + (on ? "on":"off");
  el.innerText = on ? "Connected":"Disconnected";
}

async function connectPort(){
  try{
    port = await navigator.serial.requestPort();
    await port.open({ baudRate:115200 });

    let info = port.getInfo();
    let vid = info.usbVendorId || 0;
    let pid = info.usbProductId || 0;

    let chip = detectChip(vid,pid);
    let trust = trustLevel(chip);

    let record = {
      timestamp: new Date().toLocaleString(),
      vid: vid,
      pid: pid,
      chip: chip,
      trust: trust
    };

    devices.push(record);
    showDevice(record);
    renderHistory();
    setStatus(true);

    addLog("USB connected: " + chip);
  }catch(e){
    addLog("Connection failed");
  }
}

async function disconnectPort(){
  try{
    if(port){
      await port.close();
      port = null;
    }
    setStatus(false);
    addLog("USB disconnected");
  }catch(e){
    addLog("Disconnect error");
  }
}

function detectChip(vid,pid){
  if(vid === 4292) return "CP2102 Silicon Labs";
  if(vid === 6790) return "CH340 WCH";
  if(vid === 1027) return "FTDI FT232";
  if(vid === 3034) return "Espressif Native USB";
  return "Unknown Device (VID:"+vid+")";
}

function trustLevel(chip){
  if(chip.includes("Espressif")) return "TRUSTED: ESP32 SOURCE";
  if(chip.includes("CP2102")) return "PROBABLE: DEV BOARD";
  return "CAUTION: UNVERIFIED CHIPSET";
}

function showDevice(d){
  document.getElementById("deviceBox").innerHTML = `
    <b>TIME:</b> ${d.timestamp}<br>
    <b>VID:</b> ${d.vid} | <b>PID:</b> ${d.pid}<br>
    <b>CHIP:</b> ${d.chip}
  `;
  document.getElementById("trustBox").innerHTML = d.trust;
}


function renderHistory(){

  let html = "";

  devices.slice().reverse().forEach(d => {

    html += `
      <div class="row">
        ${d.chip}<br>

        <small>
        VID:${d.vid}
        PID:${d.pid}
        </small><br>

        ${d.mac ?
          "<small>MAC: " + d.mac + "</small><br>"
          : ""
        }

        <small>${d.timestamp}</small>
      </div>
    `;
  });

  document.getElementById("historyBox").innerHTML =
    html || "None";
}


function exportCSV(){
  if(devices.length === 0) return alert("No history");
  let csv = "Timestamp,VID,PID,Chipset,Trust\n";
  devices.forEach(d=>{
    csv += `"${d.timestamp}",${d.vid},${d.pid},"${d.chip}","${d.trust}"\n`;
  });
  let blob = new Blob([csv], {type:"text/csv"});
  let a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "usb_inspector_report.csv";
  a.click();
  addLog("CSV Report exported");
}
</script>

</body>
</html>




)rawliteral";

// ======================================================
// FONT
// ======================================================

const uint8_t font_5x7[38][5] = {

  {0x3E,0x51,0x49,0x45,0x3E},
  {0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},
  {0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},
  {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},
  {0x06,0x49,0x49,0x29,0x1E},

  {0x7C,0x12,0x11,0x12,0x7C},
  {0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},
  {0x7F,0x49,0x49,0x49,0x41},
  {0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,0x08,0x08,0x08,0x7F},
  {0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},

  {0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},
  {0x7F,0x04,0x08,0x10,0x7F},
  {0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},
  {0x3E,0x41,0x51,0x21,0x5E},
  {0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},

  {0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},
  {0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},
  {0x61,0x51,0x49,0x45,0x43},

  {0x00,0x36,0x36,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00}
};

// ======================================================
// SPI FUNCTIONS
// ======================================================

void SPI_8(uint8_t d) {

  *spi_write_len = 7;
  *spi_write_buf = d;
  *spi_cmd = SPI_USR;

  while (*spi_cmd & SPI_USR);
}

void SPI_16(uint16_t d) {

  *spi_write_len = 15;
  *spi_write_buf = ((d << 8) | (d >> 8));
  *spi_cmd = SPI_USR;

  while (*spi_cmd & SPI_USR);
}

// ======================================================
// LCD
// ======================================================

void WR_REG(uint8_t r) {

  CS_L;
  DC_L;

  SPI_8(r);

  DC_H;
  CS_H;
}

void setWin(
  uint16_t x,
  uint16_t y,
  uint16_t w,
  uint16_t h
) {

  WR_REG(0x2A);

  CS_L;

  SPI_16(x);
  SPI_16(x+w-1);

  CS_H;

  WR_REG(0x2B);

  CS_L;

  SPI_16(y);
  SPI_16(y+h-1);

  CS_H;

  WR_REG(0x2C);
}

void fill(
  uint16_t x,
  uint16_t y,
  uint16_t w,
  uint16_t h,
  uint16_t color
) {

  setWin(x,y,w,h);

  CS_L;
  DC_H;

  for(uint32_t i=0;i<(uint32_t)w*h;i++) {

    SPI_16(color);
  }

  CS_H;
}

// ======================================================
// TEXT
// ======================================================

void drawC(
  char c,
  uint16_t x,
  uint16_t y,
  uint16_t col,
  int scale
) {

  int i = 37;

  if(c >= '0' && c <= '9')
    i = c - '0';

  else if(c >= 'A' && c <= 'Z')
    i = c - 'A' + 10;

  else if(c >= 'a' && c <= 'z')
    i = c - 'a' + 10;

  else if(c == ':')
    i = 36;

  for(int col_i=0; col_i<5; col_i++) {

    for(int row_i=0; row_i<8; row_i++) {

      if(font_5x7[i][col_i] & (1 << row_i)) {

        fill(
          x+(col_i*scale),
          y+(row_i*scale),
          scale,
          scale,
          col
        );
      }
    }
  }
}

void drawS(
  const char* s,
  uint16_t x,
  uint16_t y,
  uint16_t col,
  int scale
) {

  while(*s) {

    drawC(*s++, x, y, col, scale);

    x += 6 * scale;
  }
}

void drawV(
  int n,
  uint16_t x,
  uint16_t y,
  uint16_t col,
  int scale
) {

  char b[16];

  sprintf(b, "%d", n);

  drawS(b, x, y, col, scale);
}

// ======================================================
// WIFI SNIFFER
// ======================================================

void wifi_sniffer(
  void* buf,
  wifi_promiscuous_pkt_type_t type
) {

  const wifi_promiscuous_pkt_t *pkt =
    (wifi_promiscuous_pkt_t *)buf;

  float newRssi = (float)pkt->rx_ctrl.rssi;

  wifiRssi =
    wifiRssi * 0.7 +
    newRssi * 0.3;

  static float lastRssi = -70;

  if(fabs(wifiRssi - lastRssi)
     > MOTION_THRESHOLD) {

    wifiPulseTimes[wifiPulseIndex] =
      millis();

    wifiPulseIndex =
      (wifiPulseIndex + 1) % MAX_PULSES;
  }

  lastRssi = wifiRssi;
}

// ======================================================
// TFT DASHBOARD
// ======================================================

void drawDashboard() {

  fill(0,0,320,480,BLACK);

  drawS("INSPECTOR", 60, 20, GREEN, 4);

  drawS("WIFI:", 20, 120, WHITE, 2);
  drawS("EMG:",  20, 190, WHITE, 2);
  drawS("MOVE:", 20, 260, WHITE, 2);

  drawS("STATUS:", 20, 340, WHITE, 2);
}




void updateStations() {

  wifi_sta_list_t wifi_sta_list;

  esp_wifi_ap_get_sta_list(&wifi_sta_list);

  staDeviceCount = wifi_sta_list.num;

  for(int i = 0; i < wifi_sta_list.num; i++) {

    char macStr[18];

    sprintf(
      macStr,
      "%02X:%02X:%02X:%02X:%02X:%02X",
      wifi_sta_list.sta[i].mac[0],
      wifi_sta_list.sta[i].mac[1],
      wifi_sta_list.sta[i].mac[2],
      wifi_sta_list.sta[i].mac[3],
      wifi_sta_list.sta[i].mac[4],
      wifi_sta_list.sta[i].mac[5]
    );

    staDevices[i].mac = String(macStr);
    staDevices[i].active = true;
  }
}
// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  // LCD

  pinMode(LCD_CS, OUTPUT);
  pinMode(LCD_DC, OUTPUT);
  pinMode(LCD_RST, OUTPUT);
  pinMode(LCD_BL, OUTPUT);

  digitalWrite(LCD_CS, HIGH);

  spi_bus_config_t buscfg = {
    .mosi_io_num = SPI_MOSI,
    .miso_io_num = SPI_MISO,
    .sclk_io_num = SPI_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1
  };

  spi_bus_initialize(
    HSPI_HOST,
    &buscfg,
    SPI_DMA_CH_AUTO
  );

  *spi_usr = SPI_USR_MOSI;

  digitalWrite(LCD_RST, LOW);
  delay(50);

  digitalWrite(LCD_RST, HIGH);
  delay(100);

  WR_REG(0x11);
  delay(120);

  WR_REG(0x36);

  CS_L;
  DC_H;
  SPI_8(0x48);
  CS_H;

  WR_REG(0x3A);

  CS_L;
  DC_H;
  SPI_8(0x55);
  CS_H;

  WR_REG(0x29);

  digitalWrite(LCD_BL, HIGH);

  drawDashboard();

  // ======================================================
  // WIFI AP
  // ======================================================

  WiFi.mode(WIFI_AP);

  WiFi.softAPConfig(
    local_ip,
    gateway,
    subnet
  );

  WiFi.softAP(AP_NAME, AP_PASS);
ipAddress = WiFi.softAPIP().toString();
netStatus = "AP ACTIVE";


  Serial.println(WiFi.softAPIP());

  dnsServer.start(
    DNS_PORT,
    "*",
    local_ip
  );

  // ======================================================
  // PROMISCUOUS MODE
  // ======================================================

  esp_wifi_set_promiscuous(true);

  esp_wifi_set_promiscuous_rx_cb(
    &wifi_sniffer
  );

  // ======================================================
  // UDP
  // ======================================================

  udp.begin(udpPort);

  // ======================================================
  // WEB ROUTES
  // ======================================================

  server.on("/", []() {

    server.send_P(
      200,
      "text/html",
      index_html
    );
  });

  server.on("/status", []() {

  String json = "{";

  json += "\"wifi\":";
  json += String(wifiRssi,1);
  json += ",";

  json += "\"emg\":";
  json += String(emgVal,1);
  json += ",";

  json += "\"motion\":";
  json += motionActive ? "true" : "false";
  json += ",";

  json += "\"ip\":\"" + ipAddress + "\",";
  json += "\"net\":\"" + netStatus + "\",";
  json += "\"sta\":" + String(staCount) + ",";

  // DEVICES ARRAY
  json += "\"devices\":[";

  for(int i=0;i<staDeviceCount;i++) {

    json += "{";
    json += "\"mac\":\"" + staDevices[i].mac + "\"";
    json += "}";

    if(i < staDeviceCount - 1)
      json += ",";
  }

  json += "]";

  json += "}";

  server.send(200, "application/json", json);
});

  // Captive Portal

  server.on("/generate_204", []() {
    server.sendHeader("Location","/");
    server.send(302,"text/plain","");
  });

  server.on("/hotspot-detect.html", []() {
    server.sendHeader("Location","/");
    server.send(302,"text/plain","");
  });

  server.onNotFound([]() {
    server.sendHeader("Location","/");
    server.send(302,"text/plain","");
  });

  server.begin();

  Serial.println("SERVER STARTED");
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  dnsServer.processNextRequest();

  server.handleClient();

  unsigned long now = millis();

// ======================================================
// NETWORK STATUS UPDATE
// ======================================================

updateStations();

staCount = staDeviceCount;

if(staCount > 0) {
  netStatus = "CONNECTED";
} else {
  netStatus = "WAITING";
}

  // ======================================================
  // UDP PING
  // ======================================================

  static unsigned long lastPing = 0;

  if(now - lastPing >= 100) {

    lastPing = now;

    udp.beginPacket(
      broadcastIp,
      udpPort
    );

udp.write((const uint8_t*)"PING", 4);

    udp.endPacket();
  }

  // ======================================================
  // EMG
  // ======================================================

  int rawEmg = analogRead(EMG_PIN);

  emgVal =
    emgVal * 0.9 +
    rawEmg * 0.1;

  emgActive =
    emgVal > EMG_THRESHOLD;

  // ======================================================
  // MOTION
  // ======================================================

  motionActive = false;

  for(int i=0;i<MAX_PULSES;i++) {

    if(wifiPulseTimes[i] >
       (now - MOTION_HOLD)) {

      motionActive = true;
    }
  }

  // ======================================================
  // TFT UPDATE
  // ======================================================

  static unsigned long lastScreen = 0;

  if(now - lastScreen >= 500) {

    lastScreen = now;

    // Clear Values

// Clear Values

fill(150,110,140,30,BLACK); // WIFI
fill(150,180,140,30,BLACK); // EMG
fill(150,250,140,30,BLACK); // MOVE
fill(150,330,160,40,BLACK); // STATUS

// CLEAR LOWER NETWORK AREA
fill(80,370,240,120,BLACK);

    // Draw Values

// IP ADDRESS
drawS("IP:", 20, 380, WHITE, 2);
drawS(ipAddress.c_str(), 90, 380, GREEN, 2);

// NETWORK STATUS
drawS("NET:", 20, 420, WHITE, 2);
drawS(netStatus.c_str(), 90, 420, GREEN, 2);

// STA COUNT
drawS("STA:", 20, 445, WHITE, 2);
drawV(staCount, 90, 445, GREEN, 2);


    drawV(
      (int)fabs(wifiRssi),
      150,
      120,
      GREEN,
      2
    );

    drawV(
      (int)emgVal,
      150,
      190,
      WHITE,
      2
    );

    drawS(
      motionActive ? "YES" : "NO",
      150,
      260,
      motionActive ? RED : GREEN,
      2
    );

    // STATUS

    if(emgActive) {

      drawS(
        "STRONG",
        150,
        340,
        RED,
        3
      );

    } else if(motionActive) {

      drawS(
        "MOTION",
        150,
        340,
        BLUE,
        3
      );

    } else {

      drawS(
        "CLEAR",
        150,
        340,
        GREEN,
        3
      );
    }

    // SERIAL DEBUG

    Serial.print("RSSI: ");
    Serial.print(wifiRssi);

    Serial.print(" | EMG: ");
    Serial.print(emgVal);

    Serial.print(" | Motion: ");
    Serial.println(
      motionActive ? "YES" : "NO"
    );
  }

  delay(10);
}
