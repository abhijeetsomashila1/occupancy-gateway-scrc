#include <BLEDevice.h>
#include <BLEScan.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

// Polling interval (matches nRF52's 10-second timer)
constexpr uint32_t POLL_INTERVAL_MS = 10000;

// ===================== GLOBAL VARIABLES =====================
BLEAddress zoneAddresses[ZONE_COUNT];
bool occupancy[ZONE_COUNT] = {false};        // true = sensor seen since last poll
bool previousPollStates[ZONE_COUNT] = {false}; // for optional change detection
BLEScan* pScan = nullptr;

// ===================== BLE CALLBACK =====================
class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    BLEAddress addr = advertisedDevice.getAddress();

    for (int i = 0; i < ZONE_COUNT; i++) {
      if (addr == zoneAddresses[i]) {
        // Mark this zone as occupied (will be cleared at next poll)
        occupancy[i] = true;
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

// ===================== BLE SCAN TASK =====================
void bleScanTask(void* parameter) {
  Serial.println("BLE Scan Task started. Continuous scanning...");
  pScan->start(0, false);   // blocks forever – perfect
  vTaskDelete(NULL);
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("BLE Occupancy – nRF52-style polling (JSON output)");

  // Convert MAC strings to BLEAddress
  for (int i = 0; i < ZONE_COUNT; i++) {
    zoneAddresses[i] = BLEAddress(zones[i].mac);
    Serial.printf("Zone %d: %s\n", i+1, zones[i].name);
  }

  // Initialise BLE
  BLEDevice::init("ESP32-Occupancy");
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyCallbacks(), true);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(100);

  // Create continuous scan task on core 0
  xTaskCreatePinnedToCore(bleScanTask, "BLEScan", 4096, NULL, 1, NULL, 0);
  Serial.println("BLE scan task created. Main loop running.");
}

// ===================== MAIN LOOP =====================
void loop() {
  static unsigned long lastPollTime = 0;
  unsigned long now = millis();

  // Every POLL_INTERVAL_MS, publish JSON and reset occupancy
  if (now - lastPollTime >= POLL_INTERVAL_MS) {
    lastPollTime = now;

    // Build and send JSON
    String json = buildStateJSON();
    Serial.println(json);

    // Reset occupancy (clear all bits) – just like nRF52 does
    for (int i = 0; i < ZONE_COUNT; i++) {
      occupancy[i] = false;
    }
  }

  delay(50);   // small delay to keep loop responsive
}