#include <Arduino.h>

/* =========================
   CLASSIC BLUETOOTH (GAP)
========================= */
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

bool classicScanning = false;

/* =========================
   BLE STACK
========================= */
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

BLEScan* pBLEScan;
bool bleScanning = false;

/* =========================
   CLASSIC CALLBACK
========================= */
void btCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {

  if (event == ESP_BT_GAP_DISC_RES_EVT) {

    char addr[18];
    sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x",
            param->disc_res.bda[0], param->disc_res.bda[1],
            param->disc_res.bda[2], param->disc_res.bda[3],
            param->disc_res.bda[4], param->disc_res.bda[5]);

    int rssi = 0;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
      if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_RSSI) {
        rssi = *(int8_t *)(param->disc_res.prop[i].val);
      }
    }

    Serial.print("BT|");
    Serial.print(addr);
    Serial.print("|RSSI|");
    Serial.println(rssi);
  }

  if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {

    if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
      Serial.println("BT_BEGIN");
    }

    if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
      Serial.println("BT_END");
      classicScanning = false;
    }
  }
}

/* =========================
   BLE CALLBACK (FIXED)
========================= */
class MyBLECallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    int rssi = device.getRSSI();
    
    // If name exists, use it; otherwise, use the MAC address
    String displayName = device.haveName() ? device.getName().c_str() : device.getAddress().toString().c_str();

    Serial.print("BLE|");
    Serial.print(displayName); // Now shows name OR Mac
    Serial.print("|RSSI|");
    Serial.println(rssi);
  }
};


/* =========================
   SETUP
========================= */
void setup() {
  Serial.begin(115200);

  /* ---- CLASSIC BT INIT ---- */
  btStart();
  esp_bluedroid_init();
  esp_bluedroid_enable();
  esp_bt_gap_register_callback(btCallback);

  /* ---- BLE INIT ---- */
  BLEDevice::init("");

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyBLECallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Serial.println("READY");
}

/* =========================
   SCAN FUNCTIONS
========================= */
void startClassicScan() {

  if (classicScanning) return;

  classicScanning = true;

  esp_bt_gap_start_discovery(
    ESP_BT_INQ_MODE_GENERAL_INQUIRY,
    10,
    0
  );
}

void startBLEScan() {

  if (bleScanning) return;

  bleScanning = true;

  Serial.println("BLE_BEGIN");

  pBLEScan->start(10, false);

  pBLEScan->clearResults();

  Serial.println("BLE_END");

  bleScanning = false;
}

/* =========================
   LOOP (COMMAND CONTROL)
========================= */
void loop() {

  if (Serial.available()) {

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "scanbt") {
      startClassicScan();
    }

    if (cmd == "scanble") {
      startBLEScan();
    }

    if (cmd == "scanall") {
      startClassicScan();
      delay(500);
      startBLEScan();
    }
  }

  delay(10);
}
