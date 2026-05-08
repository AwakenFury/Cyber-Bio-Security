#include <stdint.h>
#include <math.h>

#include "soc/spi_reg.h"
#include "driver/spi_master.h"

#include <WiFi.h>
#include "esp_wifi.h"

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
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>CONVERSATIONS | BIO-SYNC COMMS</title>
    <link href="https://googleapis.com" rel="stylesheet">
    <style>
        :root {
            --neon-cyan: #00eaff; --neon-red: #ff3b3b; --neon-green: #00ff88;
            --bg-dark: #030712; --panel-bg: rgba(10, 25, 41, 0.95);
        }

        body {
            margin: 0; background: var(--bg-dark); font-family: 'Rajdhani', sans-serif;
            color: var(--neon-cyan); height: 100vh; overflow: hidden;
            display: flex; flex-direction: column; text-transform: uppercase;
        }

        body::before {
            content: " "; position: fixed; inset: 0;
            background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%);
            background-size: 100% 4px; z-index: 1000; pointer-events: none;
        }

        /* --- STAGES --- */
        #login-stage {
            position: absolute; inset: 0; z-index: 5000;
            display: flex; justify-content: center; align-items: center;
            background: radial-gradient(circle at center, #07152e 0%, #02050f 100%);
        }
        .login-box { width: 90%; max-width: 400px; padding: 30px; background: var(--panel-bg); border: 1px solid var(--neon-cyan); text-align: center; }
        .team-selector { display: grid; grid-template-columns: repeat(5, 1fr); gap: 4px; margin: 20px 0; }
        .team-btn { padding: 10px 0; font-size: 0.5rem; background: rgba(0, 234, 255, 0.1); border: 1px solid var(--neon-cyan); color: var(--neon-cyan); cursor: pointer; }
        .team-btn.active { background: var(--neon-cyan); color: black; font-weight: bold; }
        input { width: 100%; padding: 12px; margin-bottom: 10px; background: black; color: white; border: 1px solid #112233; box-sizing: border-box; font-family: 'Rajdhani'; }

        #loading-stage { display: none; position: absolute; inset: 0; z-index: 4000; flex-direction: column; justify-content: center; align-items: center; background: var(--bg-dark); }
        #hud-container { display: none; height: 100vh; grid-template-columns: 320px 1fr 320px; grid-template-rows: 80px 1fr 150px; gap: 15px; padding: 20px; box-sizing: border-box; }

        .glass-panel { background: rgba(0, 0, 0, 0.4); border-left: 2px solid var(--neon-cyan); padding: 15px; }
        .terminal-output { flex: 1; padding: 15px; font-size: 0.85rem; color: var(--neon-green); overflow-y: auto; background: rgba(0,0,0,0.3); border: 1px solid rgba(0, 234, 255, 0.1); }

        /* --- BIOSYNC CHAT MODAL --- */
        #biosync-modal {
            position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%);
            width: 90%; max-width: 450px; height: 500px; background: var(--panel-bg);
            border: 2px solid var(--neon-cyan); z-index: 6000; display: none;
            flex-direction: column; box-shadow: 0 0 50px rgba(0, 234, 255, 0.3);
        }
        .chat-header { padding: 15px; border-bottom: 1px solid var(--neon-cyan); font-family: 'Orbitron'; font-size: 0.8rem; display: flex; justify-content: space-between; }
        #chat-window { flex: 1; overflow-y: auto; padding: 15px; font-size: 0.8rem; display: flex; flex-direction: column; gap: 10px; }
        .msg { padding: 8px; border-left: 2px solid var(--neon-cyan); background: rgba(255,255,255,0.03); }
        .msg.peer { border-left-color: var(--neon-green); color: var(--neon-green); }
        .chat-input-area { display: flex; padding: 10px; border-top: 1px solid rgba(0, 234, 255, 0.2); }
        .chat-input-area input { flex: 1; margin: 0; border: none; background: transparent; color: white; padding: 10px; }

        #fugu-alert { position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); width: 90%; max-width: 450px; background: rgba(20, 5, 5, 0.98); border: 2px solid var(--neon-red); padding: 30px; display: none; z-index: 7000; text-align: center; }

        .hud-btn { background: transparent; border: 1px solid var(--neon-cyan); color: var(--neon-cyan); padding: 10px; cursor: pointer; text-transform: uppercase; font-family: 'Rajdhani'; font-weight: bold; }
        @media (max-width: 900px) { #hud-container { display: flex; flex-direction: column; overflow-y: auto; height: auto; } }
    </style>
</head>
<body id="main-body">

    <!-- LOGIN -->
    <div id="login-stage">
        <div class="login-box">
            <h1 style="color: var(--neon-red); letter-spacing: 5px;">DEFENSE LINK</h1>
            <div class="team-selector">
                <button class="team-btn" onclick="selectTeam(this)">ALPHA</button>
                <button class="team-btn" onclick="selectTeam(this)">BRAVO</button>
                <button class="team-btn active" onclick="selectTeam(this)">CHARLIE</button>
                <button class="team-btn" onclick="selectTeam(this)">DELTA</button>
                <button class="team-btn" onclick="selectTeam(this)">ECHO</button>
            </div>
            <input type="text" id="username" placeholder="OPERATOR_ID: AWAKEN_FURY">
            <input type="password" placeholder="ACCESS_KEY">
            <button class="hud-btn" style="width: 100%;" onclick="startLogin()">SECURE LOGIN</button>
        </div>
    </div>

    <!-- LOADING -->
    <div id="loading-stage">
        <h2 style="font-family: 'Orbitron';">SYNCING TEAM FREQUENCY...</h2>
    </div>

    <!-- HUD -->
    <div id="hud-container">
        <div style="grid-column: 2; text-align: center; font-family: 'Orbitron'; font-size: 1.5rem;">CONVERSATIONS</div>
        
        <div class="left-col" style="grid-row: 2; display: flex; flex-direction: column; gap: 10px;">
            <div class="glass-panel"><h3>SQUAD STATUS</h3><div id="team-info">TEAM: CHARLIE</div></div>
        </div>

        <div class="center-viewport" style="grid-row: 2; display: flex; flex-direction: column;">
            <div class="terminal-output" id="main-terminal"></div>
        </div>

        <div class="right-col" style="grid-row: 2; display: flex; flex-direction: column; gap: 10px;">
            <div class="glass-panel">
                <h3>SUB-SYSTEMS</h3>
                <button class="hud-btn" style="width:100%;" onclick="openBioSync()">BIO-SYNC COMMS</button>
            </div>
        </div>
    </div>

    <!-- BIOSYNC TEXTING MODAL -->
    <div id="biosync-modal">
        <div class="chat-header">
            <span>BIO-SYNC: <span id="chat-team-label"></span> FREQUENCY</span>
            <span style="cursor:pointer" onclick="closeBioSync()">[X]</span>
        </div>
        <div id="chat-window"></div>
        <div class="chat-input-area">
            <input type="text" id="chat-input" placeholder="Type message..." onkeypress="if(event.key==='Enter') sendMsg()">
            <button class="hud-btn" onclick="sendMsg()">SEND</button>
        </div>
    </div>

    <!-- FUGU ALERT -->
    <div id="fugu-alert">
        <h3 style="color: var(--neon-red);">⚠ BREACH DETECTED</h3>
        <div id="fugu-text" style="color: white; margin-bottom: 20px;"></div>
        <button class="hud-btn" style="border-color: var(--neon-red); color: var(--neon-red);" onclick="this.parentElement.style.display='none'">CLOSE</button>
    </div>

    <script>
        let selectedTeam = "CHARLIE";
        let user = "AWAKEN_FURY";

        // Simulated database of messages
        const teamMessages = { "ALPHA": [], "BRAVO": [], "CHARLIE": [], "DELTA": [], "ECHO": [] };

        function selectTeam(btn) {
            document.querySelectorAll('.team-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            selectedTeam = btn.innerText;
        }

        function startLogin() {
            user = document.getElementById('username').value || "OPERATOR";
            document.getElementById('login-stage').style.display = 'none';
            document.getElementById('loading-stage').style.display = 'flex';
            setTimeout(() => {
                document.getElementById('loading-stage').style.display = 'none';
                document.getElementById('hud-container').style.display = 'grid';
                document.getElementById('team-info').innerText = "TEAM: " + selectedTeam;
                document.getElementById('chat-team-label').innerText = selectedTeam;
                
                // Add an initial greeting from a peer
                teamMessages[selectedTeam].push({sender: "SYSTEM", text: "Team frequency " + selectedTeam + " established."});
                teamMessages[selectedTeam].push({sender: "OPERATOR_02", text: "I'm in position. Sector 7 looks dark.", type: "peer"});
            }, 2000);
        }

        function openBioSync() {
            const win = document.getElementById('chat-window');
            win.innerHTML = '';
            teamMessages[selectedTeam].forEach(m => {
                const div = document.createElement('div');
                div.className = "msg " + (m.type || "");
                div.innerHTML = `<strong>[${m.sender}]</strong>: ${m.text}`;
                win.appendChild(div);
            });
            document.getElementById('biosync-modal').style.display = 'flex';
        }

        function sendMsg() {
            const input = document.getElementById('chat-input');
            if(!input.value) return;
            
            const newMsg = { sender: user, text: input.value };
            teamMessages[selectedTeam].push(newMsg);
            
            openBioSync(); // Refresh window
            input.value = '';
            document.getElementById('chat-window').scrollTop = 9999;
        }

        function closeBioSync() { document.getElementById('biosync-modal').style.display = 'none'; }
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
  json += "\"sta\":" + String(staCount);

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

staCount = WiFi.softAPgetStationNum();

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

    fill(150,110,140,30,BLACK);
    fill(150,180,140,30,BLACK);
    fill(150,250,140,30,BLACK);
    fill(150,330,160,40,BLACK);

    // Draw Values

// IP ADDRESS
drawS("IP:", 20, 380, WHITE, 2);
drawS(ipAddress.c_str(), 90, 380, GREEN, 2);

// NETWORK STATUS
drawS("NET:", 20, 420, WHITE, 2);
drawS(netStatus.c_str(), 90, 420, GREEN, 2);

// STA COUNT
drawS("STA:", 20, 460, WHITE, 2);
drawV(staCount, 90, 460, GREEN, 2);


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
