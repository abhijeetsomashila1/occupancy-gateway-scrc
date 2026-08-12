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
constexpr uint32_t POLL_INTERVAL_MS = 10000;
constexpr uint32_t SCAN_DURATION_SEC = 10;

// ===================== GLOBAL VARIABLES =====================
BLEAddress zoneAddresses[ZONE_COUNT];
bool occupancy[ZONE_COUNT] = {false};
BLEScan* pScan = nullptr;

// Helper: print raw data as hex
void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    if (i < len - 1) Serial.print(" ");
  }
}

// ===================== BLE CALLBACK =====================
class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    BLEAddress addr = advertisedDevice.getAddress();
    for (int i = 0; i < ZONE_COUNT; i++) {
      if (addr == zoneAddresses[i]) {
        if (!occupancy[i]) {
          occupancy[i] = true;

          Serial.printf("=== %s DETECTED ===\n", zones[i].name);
          Serial.printf("  MAC: %s\n", addr.toString().c_str());
          Serial.printf("  RSSI: %d dBm\n", advertisedDevice.getRSSI());

          // ---- Service UUID ----
          String uuidStr = advertisedDevice.getServiceUUID().toString();
          if (uuidStr.length() > 0) {
            Serial.printf("  Service UUID: %s\n", uuidStr.c_str());
          } else {
            Serial.println("  Service UUID: (none)");
          }

          // ---- Manufacturer Data (raw) ----
          String manuf = advertisedDevice.getManufacturerData();
          if (manuf.length() > 0) {
            Serial.print("  Manufacturer Data (hex): ");
            printHex((const uint8_t*)manuf.c_str(), manuf.length());
            Serial.println();
          } else {
            Serial.println("  Manufacturer Data: (none)");
          }

          // ---- Service Data (raw) ----
          String svcData = advertisedDevice.getServiceData();
          if (svcData.length() > 0) {
            Serial.print("  Service Data (hex): ");
            printHex((const uint8_t*)svcData.c_str(), svcData.length());
            Serial.println();
          } else {
            Serial.println("  Service Data: (none)");
          }

          Serial.println(); // blank line
        }
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
  Serial.println("BLE Occupancy – Raw Data Output + Polling");

  for (int i = 0; i < ZONE_COUNT; i++) {
    zoneAddresses[i] = BLEAddress(zones[i].mac);
    Serial.printf("Zone %d: %s (MAC: %s)\n", i+1, zones[i].name, zones[i].mac);
  }

  BLEDevice::init("ESP32-Occupancy");
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyCallbacks(), true);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(100);

  pScan->start(SCAN_DURATION_SEC, false);
}

// ===================== MAIN LOOP =====================
void loop() {
  static unsigned long lastPollTime = 0;
  unsigned long now = millis();

  if (now - lastPollTime >= POLL_INTERVAL_MS) {
    lastPollTime = now;

    String json = buildStateJSON();
    Serial.println(json);

    for (int i = 0; i < ZONE_COUNT; i++) {
      occupancy[i] = false;
    }
  }

  pScan->clearResults();
  pScan->stop();
  delay(5);
  pScan->start(SCAN_DURATION_SEC, false);
}