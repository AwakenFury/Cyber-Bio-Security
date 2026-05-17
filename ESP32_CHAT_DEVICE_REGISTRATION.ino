#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// =============================
// ACCESS POINT CONFIG
// =============================
const char* AP_NAME = "TACTICAL_NET";
const char* AP_PASS = "12345678";

// =============================
// SERVER + WEBSOCKET
// =============================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// =============================
// SESSION STRUCT
// =============================
struct Session {

  uint32_t clientId;

  String id;

  String user;

  String team;

  bool active;
};

Session sessions[10];
int sessionCount = 0;

// =============================
// HTML UI (HUD)
// =============================
const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>TACTICAL COMMS</title>
<style>

body{
  background:#000;
  color:#0f0;
  font-family:monospace;
  margin:0;
  padding:10px;
}

#chat{
  height:40vh;
  overflow:auto;
  border:1px solid #0f0;
  padding:10px;
  margin-bottom:10px;
}

#cross-panel{
  border:1px solid #0ff;
  padding:10px;
  margin-top:10px;
}

#cross-chat{
  height:20vh;
  overflow:auto;
  border:1px solid #0ff;
  padding:10px;
  margin-bottom:10px;
}

#incoming-alert{
  display:none;
  position:fixed;
  top:20px;
  right:20px;
  background:#111;
  border:2px solid red;
  padding:20px;
  z-index:999;
}

input,button{
  background:#000;
  color:#0f0;
  border:1px solid #0f0;
  padding:8px;
}

</style>

</head>

<body>

<h3>TACTICAL COMMS LINK</h3>

<div>
<select id="team">
<option>ALPHA</option>
<option>BRAVO</option>
<option>CHARLIE</option>
</select>

<input id="user" placeholder="USERNAME">
<button onclick="login()">LOGIN</button>
</div>

<hr>

<div id="chat"></div>

<div id="cross-panel">

<h3>CROSS TEAM CHANNEL</h3>

<div id="cross-chat"></div>

<input id="cross-input">

<button onclick="sendCrossMsg()">
SEND
</button>

</div>

<div id="incoming-alert">

<div id="incoming-text"></div>

<button onclick="acceptCross()">
ACCEPT
</button>

<button onclick="denyCross()">
DENY
</button>

</div>


<input id="msg" placeholder="message">
<button onclick="sendMsg()">SEND</button>

<script>

let ws;
let team = "ALPHA";
let user = "";

function login(){

  team = document.getElementById("team").value;
  user = document.getElementById("user").value || "OPERATOR";

  ws = new WebSocket("ws://" + location.host + "/ws");

  ws.onopen = () => {

    ws.send(JSON.stringify({
      action:"login",
      mac:"WEB_" + Math.random(),
      user:user,
      team:team
    }));

  };

  ws.onmessage = (e) => {

  let msg = JSON.parse(e.data);

  // =========================
  // NORMAL TEAM CHAT
  // =========================
  if(msg.type === "chat" && msg.team === team){

    add(
      "[" + msg.team + "] " +
      msg.from + ": " +
      msg.text
    );

  }

  // =========================
  // SYSTEM MESSAGES
  // =========================
  if(msg.type === "system"){

    add("[SYS] " + msg.text);

  }

  // =========================
  // CROSS TEAM CHAT
  // =========================
  if(msg.type === "cross_chat"){

    addCross(
      "[" + msg.fromTeam + "] " +
      msg.from + ": " +
      msg.text
    );

  }

  // =========================
  // CROSS TEAM REQUEST ALERT
  // =========================
  if(msg.type === "cross_request"){

    document.getElementById("incoming-alert")
      .style.display = "block";

    document.getElementById("incoming-text")
      .innerHTML =
        "Incoming request from " +
        msg.from;

  }

};

}

function sendMsg(){
  let text = document.getElementById("msg").value;

  ws.send(JSON.stringify({
    action:"chat",
    from:user,
    team:team,
    text:text
  }));

  document.getElementById("msg").value = "";
}


function add(t){

  let d = document.createElement("div");

  d.innerHTML = t;

  document.getElementById("chat")
    .appendChild(d);
}


// =========================
// CROSS CHAT WINDOW
// =========================
function addCross(t){

  let d = document.createElement("div");

  d.innerHTML = t;

  document.getElementById("cross-chat")
    .appendChild(d);
}


// =========================
// SEND CROSS TEAM MESSAGE
// =========================
function sendCrossMsg(){

  let text =
    document.getElementById("cross-input").value;

  if(!text) return;

  ws.send(JSON.stringify({

    action:"cross_chat",

    from:user,

    fromTeam:team,

    text:text

  }));

  document.getElementById("cross-input").value = "";
}


// =========================
// ACCEPT CROSS REQUEST
// =========================
function acceptCross(){

  add("[SYSTEM] Cross-team request accepted");

  document.getElementById("incoming-alert")
    .style.display = "none";
}


// =========================
// DENY CROSS REQUEST
// =========================
function denyCross(){

  add("[SYSTEM] Cross-team request denied");

  document.getElementById("incoming-alert")
    .style.display = "none";
}
</script>

</body>
</html>

)rawliteral";

// =============================
// SEND TO SPECIFIC TEAM
// =============================
void sendToTeam(String team, String msg) {

  for(int i = 0; i < sessionCount; i++) {

    if(
      sessions[i].active &&
      sessions[i].team == team
    ) {

      AsyncWebSocketClient *c =
        ws.client(sessions[i].clientId);

      if(c && c->status() == WS_CONNECTED) {

        c->text(msg);

      }
    }
  }
}




// =============================
// HANDLE WEBSOCKET EVENTS
// =============================
void onWsEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {

  if (type == WS_EVT_DATA) {

    AwsFrameInfo *info = (AwsFrameInfo*)arg;

    if (info->final && info->index == 0 && info->len == len) {

      String msg = String((char*)data).substring(0, len);

      StaticJsonDocument<256> doc;
      deserializeJson(doc, msg);

      String action = doc["action"];

      // =============================
      // LOGIN
      // =============================
      if (action == "login") {

        String mac = doc["mac"];
        String user = doc["user"];
        String team = doc["team"];

       sessions[sessionCount++] = {

  client->id(),

  mac,

  user,

  team,

  true
};

        StaticJsonDocument<200> out;
        out["type"] = "system";
        out["text"] = user + " joined " + team;

        String output;
        serializeJson(out, output);

        ws.textAll(output);
      }

      // =============================
      // CHAT
      // =============================
// =============================
// CHAT
// =============================
if(action == "chat") {

  String from = doc["from"];
  String team = doc["team"];
  String text = doc["text"];

  StaticJsonDocument<256> out;

  out["type"] = "chat";
  out["from"] = from;
  out["team"] = team;
  out["text"] = text;

  String output;

  serializeJson(out, output);

  sendToTeam(team, output);
}

// =============================
// CROSS CHAT
// =============================
if(action == "cross_chat") {

  String from = doc["from"];
  String fromTeam = doc["fromTeam"];
  String text = doc["text"];

  StaticJsonDocument<256> out;

  out["type"] = "cross_chat";
  out["from"] = from;
  out["fromTeam"] = fromTeam;
  out["text"] = text;

  String output;

  serializeJson(out, output);

  ws.textAll(output);
}

    }
  }
}

// =============================
// SETUP
// =============================
void setup() {

  Serial.begin(115200);

  WiFi.softAP(AP_NAME, AP_PASS);
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.begin();

  Serial.println("TACTICAL COMMS READY");
}

// =============================
// LOOP
// =============================
void loop() {
  ws.cleanupClients();
}
