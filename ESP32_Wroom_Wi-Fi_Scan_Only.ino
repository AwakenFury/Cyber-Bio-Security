#include <WiFi.h>

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);

  Serial.println("READY");
}

void loop() {

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "scanwifi") {
      scanWiFi();
    }
  }

  delay(10);
}

void scanWiFi() {

  int n = WiFi.scanNetworks();

  Serial.println("BEGIN");

  for (int i = 0; i < n; i++) {

    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";

    Serial.print("SSID|");
    Serial.print(ssid);
    Serial.print("|RSSI|");
    Serial.print(rssi);
    Serial.print("|ENC|");
    Serial.println(enc);

    delay(20);
  }

  Serial.println("END");
}
