#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

#include <BLEDevice.h>
#include <BLEScan.h>

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
constexpr uint32_t SENSOR_TIMEOUT_MS = 5000;  // adjust to your sensor's advertisement rate

// ===================== GLOBAL VARIABLES =====================
BLEAddress zoneAddresses[ZONE_COUNT];
unsigned long zoneLastSeenAt[ZONE_COUNT];
unsigned long lastHeartbeat = 0;
bool states[ZONE_COUNT] = {false};          // current presence states
bool previousStates[ZONE_COUNT] = {false};  // for change detection

// ===================== BLE CALLBACK =====================
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    BLEAddress addr = advertisedDevice.getAddress();
    unsigned long now = millis();

    // Check if this address matches any of our zones
    for (int i = 0; i < ZONE_COUNT; i++) {
      if (addr == zoneAddresses[i]) {
        zoneLastSeenAt[i] = now;
        // Optional debug: print when match occurs
        // Serial.printf("✅ MATCH: %s seen!\n", zones[i].name);
        break;
      }
    }
  }
};

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting BLE Occupancy Gateway (non-blocking)");

  // Convert MAC strings to BLEAddress objects
  for (int i = 0; i < ZONE_COUNT; i++) {
    zoneAddresses[i] = BLEAddress(zones[i].mac);
    Serial.printf("Zone %d MAC: %s\n", i+1, zones[i].mac);
  }

  // Initially mark all sensors as unseen (set timestamp to a very old value)
  unsigned long now = millis();
  for (int i = 0; i < ZONE_COUNT; i++) {
    zoneLastSeenAt[i] = now - SENSOR_TIMEOUT_MS - 1000;  // definitely expired
  }

  // Initialise BLE
  BLEDevice::init("ESP32-Occupancy");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true); // duplicates allowed
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(100);

  // Start continuous scanning NON‑BLOCKING (is_continue = true)
  bool started = pScan->start(0, true);   // duration 0 = indefinite, true = return immediately
  Serial.printf("Scan started: %s\n", started ? "true" : "false");
}

// ===================== LOOP =====================
void loop() {
  unsigned long now = millis();

  // Heartbeat every 5 seconds to prove loop is running
  if (now - lastHeartbeat > 5000) {
    Serial.println("--- Heartbeat: loop alive ---");
    lastHeartbeat = now;
  }

  // Evaluate presence for each zone
  for (int i = 0; i < ZONE_COUNT; i++) {
    bool present = (now - zoneLastSeenAt[i]) <= SENSOR_TIMEOUT_MS;
    states[i] = present;

    // Print only on change
    if (present != previousStates[i]) {
      previousStates[i] = present;
      Serial.printf("%s -> %d\n", zones[i].name, present ? 1 : 0);
    }
  }

  delay(50);  // small delay to keep the loop responsive
}