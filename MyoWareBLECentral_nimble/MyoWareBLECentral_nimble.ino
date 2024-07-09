#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <MyoWare.h>
#include <vector>

// debug parameters
const bool debugLogging = false; // set to true for verbose logging to serial

std::vector<BLEAddress> vecMyoWareShields;

// MyoWare class object
MyoWare myoware;

// UUIDs for the MyoWare service and characteristic
const char *myoWareServiceUUID = "EC3AF789-2154-49F4-A9FC-BC6C88E9E930";
const char *myoWareCharacteristicUUID = "F3A56EDF-8F1E-4533-93BF-5601B2E91308";

// Function to read BLE data (stub for illustration)
double ReadBLEData(BLERemoteCharacteristic* pRemoteCharacteristic) {
  String value = pRemoteCharacteristic->readValue();  // Use String type
  return value.toFloat();  // Convert String to float
}

// Function to print peripheral info (stub for illustration)
void PrintPeripheralInfo(BLEClient* client) {
  Serial.print("Peripheral: ");
  Serial.println(client->getPeerAddress().toString().c_str());
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(myoWareServiceUUID))) {
      if (debugLogging) {
        Serial.print("Found MyoWare Wireless Shield: ");
        Serial.println(advertisedDevice.toString().c_str());
      }

      // Create BLE client and connect to device
      BLEClient *pClient = BLEDevice::createClient();
      if (pClient->connect(&advertisedDevice)) {  // Pass pointer to advertisedDevice
        if (debugLogging) {
          Serial.print("Connected to ");
          Serial.println(advertisedDevice.getAddress().toString().c_str());
        }

        // Get remote service
        BLERemoteService *pRemoteService = pClient->getService(myoWareServiceUUID);
        if (pRemoteService == nullptr) {
          Serial.print("Failed to find our service UUID: ");
          Serial.println(myoWareServiceUUID);
          pClient->disconnect();
          return;
        }
        if (debugLogging) Serial.println("Remote service found");

        // Get remote characteristic
        BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(myoWareCharacteristicUUID);
        if (pRemoteCharacteristic == nullptr) {
          Serial.print("Failed to find our characteristic UUID: ");
          Serial.println(myoWareCharacteristicUUID);
          pClient->disconnect();
          return;
        }
        if (debugLogging) Serial.println("Remote characteristic found");

        // Read characteristic value
        if (pRemoteCharacteristic->canRead()) {
          if (debugLogging) Serial.println("Reading characteristic value...");
          String value = pRemoteCharacteristic->readValue();  // Use String type
          Serial.print("Read value: ");
          Serial.println(value.c_str());
        } else {
          Serial.println("Characteristic cannot be read");
        }

        // Disconnect from device
        pClient->disconnect();
        if (debugLogging) Serial.println("Disconnected from device");
      } else {
        Serial.print("Failed to connect to: ");
        Serial.println(advertisedDevice.getAddress().toString().c_str());
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(myoware.getStatusLEDPin(), OUTPUT); // initialize the built-in LED pin to indicate when a central is connected

  // begin initialization
  BLEDevice::init("");  // 初始化 BLE 设备

  if (debugLogging) {
    Serial.println("MyoWare BLE Central");
    Serial.println("-------------------");
  }

  // start scanning for MyoWare Wireless Shields
  if (debugLogging) {
    Serial.print("Scanning for MyoWare Wireless Shields: ");
    Serial.println(myoWareServiceUUID);
  }

  BLEScan *pBLEScan = BLEDevice::getScan(); // 创建新扫描对象
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);  // 设置扫描回调
  pBLEScan->setActiveScan(true); // active scan uses more power, but gets results faster
  pBLEScan->start(10, false); // scan for 10 seconds

  if (vecMyoWareShields.empty()) {
    Serial.println("No MyoWare Wireless Shields found!");
    while (1);
  }

  digitalWrite(myoware.getStatusLEDPin(), HIGH); // turn on the LED to indicate a connection

  for (auto &address : vecMyoWareShields) {
    Serial.println(address.toString().c_str());
  }
}

void loop() {
  for (auto &address : vecMyoWareShields) {
    BLEClient* shield = BLEDevice::createClient();
    if (!shield->connect(address)) {
      Serial.print("Failed to connect to MyoWare Wireless Shield with address: ");
      Serial.println(address.toString().c_str());
      delete shield;
      continue;
    }

    if (debugLogging) {
      Serial.print("Updating ");
      PrintPeripheralInfo(shield);
    }

    if (!shield->isConnected()) {
      // output zero if the Wireless shield gets disconnected
      // this ensures data capture can continue for the 
      // other shields that are connected
      Serial.print("0.0"); 
      Serial.print("\t"); 
      delete shield;
      continue;
    }

    BLERemoteService* myoWareService = shield->getService(myoWareServiceUUID);
    if (!myoWareService) {
      Serial.println("Failed finding MyoWare BLE Service!");
      shield->disconnect();
      delete shield;
      continue;
    }

    // get sensor data
    BLERemoteCharacteristic* sensorCharacteristic = myoWareService->getCharacteristic(myoWareCharacteristicUUID);
    if (!sensorCharacteristic) {
      Serial.println("Failed to find characteristic!");
      shield->disconnect();
      delete shield;
      continue;
    }

    const double sensorValue = ReadBLEData(sensorCharacteristic);
    Serial.print(sensorValue);

    if (vecMyoWareShields.size() > 1)
      Serial.print(","); 

    shield->disconnect();
    delete shield;
  }
  Serial.println("");
}