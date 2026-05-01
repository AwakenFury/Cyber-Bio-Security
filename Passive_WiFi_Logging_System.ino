#include <WiFi.h>

bool autoMode = false;
unsigned long lastScan = 0;
const int interval = 5000; 

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "start") autoMode = true;
    else if (cmd == "stop") autoMode = false;
    else if (cmd == "scanwifi") scanWiFi();
  }

  if (autoMode && (millis() - lastScan > interval)) {
    scanWiFi();
    lastScan = millis();
  }
}

void scanWiFi() {
  int n = WiFi.scanNetworks();
  Serial.println("BEGIN");
  for (int i = 0; i < n; i++) {
    Serial.printf("DATA|%s|%d|%s|%d\n", 
                  WiFi.SSID(i).c_str(), WiFi.RSSI(i), 
                  WiFi.BSSIDstr(i).c_str(), WiFi.channel(i));
  }
  Serial.println("END");
}
