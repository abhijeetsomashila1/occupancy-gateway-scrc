#include <BLEDevice.h>
#include <BLEScan.h>
#include <ArduinoJson.h>

// ===================== CONFIGURATION =====================
struct ZoneConfig {
  const char* mac;
  const char* name;
};

ZoneConfig zones[] = {
  {"68:27:19:A8:78:3C", "Zone 1"},
  {"68:27:19:A8:11:92", "Zone 2"}
};

constexpr int ZONE_COUNT = sizeof(zones) / sizeof(zones[0]);
constexpr uint32_t POLL_INTERVAL_MS = 10000;    // publish & clear every 10s
constexpr uint32_t SCAN_DURATION_SEC = 10;      // scan for 10s per cycle

// ===================== GLOBAL VARIABLES =====================
BLEAddress zoneAddresses[ZONE_COUNT];
bool occupancy[ZONE_COUNT] = {false};
bool previousPollStates[ZONE_COUNT] = {false};  // (optional) not used currently
BLEScan* pScan = nullptr;

// ===================== BLE CALLBACK =====================
class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    BLEAddress addr = advertisedDevice.getAddress();
    for (int i = 0; i < ZONE_COUNT; i++) {
      if (addr == zoneAddresses[i]) {
        occupancy[i] = true;   // mark as seen since last poll
        break;
      }
    }
  }
};

// ===================== JSON HELPER =====================
String buildStateJSON() {
  StaticJsonDocument<128> doc;
  for (int i = 0; i < ZONE_COUNT; i++) {
    doc[zones[i].name] = occupancy[i] ? 1 : 0;
  }
  String jsonStr;
  serializeJson(doc, jsonStr);
  return jsonStr;
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("BLE Occupancy – Optimised Polling");

  for (int i = 0; i < ZONE_COUNT; i++) {
    zoneAddresses[i] = BLEAddress(zones[i].mac);
    Serial.printf("Zone %d: %s\n", i+1, zones[i].name);
  }

  BLEDevice::init("ESP32-Occupancy");
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyCallbacks(), true);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(100);

  // Start first scan (blocking for SCAN_DURATION_SEC)
  pScan->start(SCAN_DURATION_SEC, false);
}

// ===================== MAIN LOOP =====================
void loop() {
  static unsigned long lastPollTime = 0;
  unsigned long now = millis();

  // ---------- Poll every 10s ----------
  if (now - lastPollTime >= POLL_INTERVAL_MS) {
    lastPollTime = now;

    // 1. Publish current occupancy
    String json = buildStateJSON();
    Serial.println(json);

    // 2. Reset occupancy (clear all bits) – like nRF52
    for (int i = 0; i < ZONE_COUNT; i++) {
      occupancy[i] = false;
    }
  }

  // ---------- Restart scan after it finishes ----------
  // The scan runs for SCAN_DURATION_SEC seconds and then returns.
  // We immediately restart it to keep listening with minimal gap.
  pScan->clearResults();        // free memory
  pScan->stop();                // safety stop (should already be stopped)
  delay(5);                     // tiny pause to let BLE settle
  pScan->start(SCAN_DURATION_SEC, false);
}