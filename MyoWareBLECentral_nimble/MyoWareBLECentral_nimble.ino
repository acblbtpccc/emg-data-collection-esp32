#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include <NimBLEClient.h>
#include <MyoWare.h>
#include <vector>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h> // To help with time formatting

// Debug parameters
const bool debugLogging = false; // Set to true for verbose logging to serial

std::vector<NimBLEAddress> vecMyoWareShields;
std::vector<NimBLEClient*> vecMyoWareClients;

// MyoWare class object
MyoWare myoware;

// UUIDs for the MyoWare service and characteristic
const char *myoWareServiceUUID = "EC3AF789-2154-49F4-A9FC-BC6C88E9E930";
const char *myoWareCharacteristicUUID = "F3A56EDF-8F1E-4533-93BF-5601B2E91308";

// JSON document for storing notifications
StaticJsonDocument<1024> doc;

// WiFi credentials
const char* ssid = "boluo-wifi";
const char* password = "54448449";

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 8 * 3600, 60000); // UTC+8 for Hong Kong and update every 60 seconds

// Function to format the date and time
String getFormattedDateTime() {
  time_t rawTime = timeClient.getEpochTime();
  return String(year(rawTime)) + "-" + String(month(rawTime)) + "-" + String(day(rawTime)) + " " +
         String(hour(rawTime)) + ":" + String(minute(rawTime)) + ":" + String(second(rawTime));
}

// Function to print peripheral info (stub for illustration)
void PrintPeripheralInfo(NimBLEClient* client) {
  Serial.print("Peripheral: ");
  Serial.println(client->getPeerAddress().toString().c_str());
}

// Notification callback function
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                    uint8_t* pData, size_t length, bool isNotify) {
  String address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
  double sensorValue = atof((char*)pData);

  // Add data to JSON document
  JsonObject peripheral = doc.createNestedObject();
  peripheral["mac"] = address;
  peripheral["value"] = sensorValue;
  peripheral["timestamp"] = getFormattedDateTime();

  // Serialize JSON document to Serial
  serializeJson(doc, Serial);
  Serial.println(""); // Print newline to separate JSON objects

  // Clear the document for the next notification
  doc.clear();
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice* advertisedDevice) {
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(myoWareServiceUUID))) {
      if (debugLogging) {
        Serial.printf("Found MyoWare Wireless Shield: %s \n", advertisedDevice->toString().c_str());
      }

      // Add device to the vector
      vecMyoWareShields.push_back(advertisedDevice->getAddress());
      if (debugLogging) {
        Serial.printf("Found MyoWare Wireless Shield: %s \n", advertisedDevice->getAddress().toString().c_str());
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(myoware.getStatusLEDPin(), OUTPUT); // Initialize the built-in LED pin to indicate when a central is connected

  // Scan for available Wi-Fi networks
  Serial.println("Scanning for available WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found.");
  } else {
    Serial.printf("%d networks found:\n", n);
    for (int i = 0; i < n; ++i) {
      Serial.printf("%d: %s (%d) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "encrypted");
    }
  }

  // Connect to Wi-Fi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int wifi_attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifi_attempts++;
    if (wifi_attempts > 20) { // After 20 attempts (~10 seconds), break and print an error message
      Serial.println("\nFailed to connect to WiFi!");
      return; // Optional: you might want to handle reconnection attempts here
    }
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start NTP client
  timeClient.begin();
  timeClient.update();

  // Begin initialization
  NimBLEDevice::init("");  // Initialize BLE device

  if (debugLogging) {
    Serial.println("MyoWare BLE Central");
    Serial.println("-------------------");
  }

  // Start scanning for MyoWare Wireless Shields
  if (debugLogging) {
    Serial.print("Scanning for MyoWare Wireless Shields: ");
    Serial.println(myoWareServiceUUID);
  }

  NimBLEScan *pBLEScan = NimBLEDevice::getScan(); // Create new scan object
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);  // Set scan callback
  pBLEScan->setActiveScan(true); // Active scan uses more power, but gets results faster
  pBLEScan->start(20, false); // Scan for 20 seconds

  if (vecMyoWareShields.empty()) {
    Serial.println("No MyoWare Wireless Shields found!");
    while (1);
  }

  for (auto& address : vecMyoWareShields) {
    NimBLEClient* shield = NimBLEDevice::createClient();
    if (!shield->connect(address)) {
      Serial.print("Failed to connect to MyoWare Wireless Shield with address: ");
      Serial.println(address.toString().c_str());
      continue;
    }

    if (debugLogging) {
      Serial.print("Connected to ");
      PrintPeripheralInfo(shield);
    }

    vecMyoWareClients.push_back(shield);

    NimBLERemoteService* myoWareService = shield->getService(myoWareServiceUUID);
    if (myoWareService) {
      NimBLERemoteCharacteristic* sensorCharacteristic = myoWareService->getCharacteristic(myoWareCharacteristicUUID);
      if (sensorCharacteristic && sensorCharacteristic->canNotify()) {
        sensorCharacteristic->subscribe(true, notifyCallback);
        if (debugLogging) {
          Serial.println("Subscribed to notifications");
        }
      }
    }
  }

  digitalWrite(myoware.getStatusLEDPin(), HIGH);  // Turn on the LED to indicate a connection
}

void loop() {
  timeClient.update(); // Update the time client periodically
  // delay(1000); // Adjust the delay as needed
}