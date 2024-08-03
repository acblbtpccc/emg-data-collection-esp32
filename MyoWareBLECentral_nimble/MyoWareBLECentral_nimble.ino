#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include <NimBLEClient.h>
#include <MyoWare.h>
#include <vector>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h> // To help with time formatting

// Debug parameters
const bool debugLogging = true; // Set to true for verbose logging to serial

static unsigned long lastMemoryCheck = 0;
bool gotConnectConfig = false; 

std::vector<NimBLEAddress> vecMyoWareShields;
std::vector<NimBLEClient*> vecMyoWareClients;

// MyoWare class object
MyoWare myoware;

// UUIDs for the MyoWare service and characteristic
const char *myoWareServiceUUID = "EC3AF789-2154-49F4-A9FC-BC6C88E9E930";
const char *myoWareCharacteristicUUID = "F3A56EDF-8F1E-4533-93BF-5601B2E91308";

// JSON document for storing notifications
StaticJsonDocument<512> doc;

// WiFi credentials
const char* ssid = "boluo-wifi";
const char* password = "54448449";

const char* host = "cdn.1f2.net";         // 替换为你的服务器地址
const uint16_t port = 80;                 // 替换为你的服务器端口
const char* configPath = "/emg_central_connect_config.json";  // 替换为配置文件的路径


struct ConnectionParams {
  uint16_t minInterval;
  uint16_t maxInterval;
  uint16_t latency;
  uint16_t timeout;
  uint16_t scanInterval;
  uint16_t scanWindow;
};

ConnectionParams connectionParams;

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 8 * 3600); // UTC+8 for Hong Kong

unsigned long ntpTime = 0; // Store the NTP epoch time
unsigned long bootTimeMillis = 0; // Store the millis() value at boot

// Function to format the date and time with milliseconds
String getFormattedDateTime() {
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - bootTimeMillis;
  time_t rawTime = ntpTime + elapsedMillis / 1000;
  unsigned long milliseconds = elapsedMillis % 1000;

  // Format the date and time string with milliseconds
  char buffer[30];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03lu",
           year(rawTime), month(rawTime), day(rawTime),
           hour(rawTime), minute(rawTime), second(rawTime), milliseconds);
  return String(buffer);
}


void printMemoryUsage() {
    Serial.print("Free heap memory: ");
    Serial.print(esp_get_free_heap_size());
    Serial.println(" bytes");
    
    Serial.print("Free internal heap memory: ");
    Serial.print(esp_get_free_internal_heap_size());
    Serial.println(" bytes");

    Serial.print("Minimum free heap memory: ");
    Serial.print(esp_get_minimum_free_heap_size());
    Serial.println(" bytes");
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

      // 检查设备地址是否已存在于向量中
      BLEAddress deviceAddress = advertisedDevice->getAddress();
      bool deviceExists = false;
      for (const auto& addr : vecMyoWareShields) {
        if (addr.equals(deviceAddress)) {
          deviceExists = true;
          break;
        }
      }

      // 如果设备地址不存在，则添加到向量中
      if (!deviceExists) {
        vecMyoWareShields.push_back(deviceAddress);
        if (debugLogging) {
          Serial.printf("Added Shield: %s \n", deviceAddress.toString().c_str());
        }
      } else {
        if (debugLogging) {
          Serial.printf("Duplicate Shield found: %s \n", deviceAddress.toString().c_str());
        }
      }
    }
  }
};

// class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
//   void onResult(BLEAdvertisedDevice* advertisedDevice) {
//     if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(myoWareServiceUUID))) {
//       if (debugLogging) {
//         Serial.printf("Found MyoWare Wireless Shield: %s \n", advertisedDevice->toString().c_str());
//       }

//       // Add device to the vector
//       vecMyoWareShields.push_back(advertisedDevice->getAddress());
//       if (debugLogging) {
//         Serial.printf("Found MyoWare Wireless Shield: %s \n", advertisedDevice->getAddress().toString().c_str());
//       }
//     }
//   }
// };

void connectToWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
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

    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    int wifi_attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      wifi_attempts++;
      if (wifi_attempts > 20) {
        Serial.println("\nFailed to connect to WiFi. Retrying...");
        break;
      }
    }
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

bool fetchConnectionParams() {
  int connectConfigAttempts = 0;
  while (!gotConnectConfig){
    if (connectConfigAttempts > 5){
      Serial.println("\nFailed to get connection config. Abort.");
      break;
    }
  // 连接到 HTTP 服务器并获取配置文件
    WiFiClient client;
    if (!client.connect(host, port)) {
      Serial.println("Connection failed");
      delay(500);
      continue;
    }

    // 发送 HTTP GET 请求
    client.print(String("GET ") + configPath + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" + 
                 "User-Agent: Arduino/1.0\r\n" +
                 "Accept: application/json\r\n" +
                 "Connection: close\r\n\r\n");

    // 等待服务器响应
    while (!client.available()) {
      delay(50);
    }

    // 读取服务器响应
    String jsonResponse;
    while (client.available()) {
      jsonResponse += client.readString();
      if (debugLogging) {
        Serial.print("Received chunk: ");
        Serial.println(jsonResponse);
      }
    }

    // 关闭连接
    client.stop();
    Serial.println("Connection closed");
    Serial.print("Full JSON Response: ");
    Serial.println(jsonResponse);

    // 查找 JSON 数据的起始位置
    int jsonStartIndex = jsonResponse.indexOf("\r\n\r\n");
    if (jsonStartIndex == -1) {
      Serial.println("JSON start not found");
      delay(1000);  // 延迟一秒后重试
      continue;
    }

    // 获取纯 JSON 部分
    String jsonData = jsonResponse.substring(jsonStartIndex + 4);
    Serial.print("Extracted JSON Data: ");
    Serial.println(jsonData);


    // 解析 JSON 响应
    StaticJsonDocument<1024> connectConfigJsonDoc;
    DeserializationError error = deserializeJson(connectConfigJsonDoc, jsonData);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      delay(500);
      connectConfigAttempts++;
      continue;
    }

    // 提取并设置连接参数
    connectionParams.minInterval = connectConfigJsonDoc["minInterval"];
    connectionParams.maxInterval = connectConfigJsonDoc["maxInterval"];
    connectionParams.latency = connectConfigJsonDoc["latency"];
    connectionParams.timeout = connectConfigJsonDoc["timeout"];
    connectionParams.scanInterval = connectConfigJsonDoc["scanInterval"];
    connectionParams.scanWindow = connectConfigJsonDoc["scanWindow"];

    // 输出解析结果
    Serial.println("Connection Parameters:");
    Serial.print("Min Interval: "); Serial.println(connectionParams.minInterval);
    Serial.print("Max Interval: "); Serial.println(connectionParams.maxInterval);
    Serial.print("Latency: "); Serial.println(connectionParams.latency);
    Serial.print("Timeout: "); Serial.println(connectionParams.timeout);
    Serial.print("Scan Interval: "); Serial.println(connectionParams.scanInterval);
    Serial.print("Scan Window: "); Serial.println(connectionParams.scanWindow);
    
    gotConnectConfig = true;
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(myoware.getStatusLEDPin(), OUTPUT); // Initialize the built-in LED pin to indicate when a central is connected

  // Attempt to connect to Wi-Fi
  connectToWiFi();

  // Start NTP client
  timeClient.begin();

  // Wait until we get a valid NTP time
  while (true) {
    timeClient.update();
    ntpTime = timeClient.getEpochTime();
    Serial.print("Get ntpTime: ");
    Serial.println(ntpTime);
    if (ntpTime >= 1609459200) { // 1609459200 corresponds to 2021-01-01 00:00:00
      break;
    }
    Serial.println("Waiting for valid NTP time...");
    delay(500);
  }
  Serial.println("\nNTP time obtained.");
  bootTimeMillis = millis(); // Record the millis() at the time we get the NTP time


  // Fetch ConnectionParams from the URL
  if (fetchConnectionParams()) {
    Serial.println("Connection parameters fetched successfully.");
  } else {
    Serial.println("Failed to fetch connection parameters.");
  }

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
    if (gotConnectConfig) {
      shield->setConnectionParams(connectionParams.minInterval, connectionParams.maxInterval, connectionParams.latency, connectionParams.timeout, connectionParams.scanInterval, connectionParams.scanWindow);
    }
  
    if (!shield->connect(address)) {
      Serial.print("Failed to connect to Shield with address: ");
      Serial.println(address.toString().c_str());
      NimBLEDevice::deleteClient(shield); 
      delay(1000);
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
        Serial.println("Subscribed to notifications");
        delay(500);
      }
    }

    // if (debugLogging) {
    // if (millis() - lastMemoryCheck > 2000) { 
    //     printMemoryUsage();
    //     lastMemoryCheck = millis();
    //   }
    // }
  }

  digitalWrite(myoware.getStatusLEDPin(), HIGH);  // Turn on the LED to indicate a connection
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi(); // Reconnect if WiFi is disconnected
  }
  
  // if (debugLogging) {
  //  if (millis() - lastMemoryCheck > 2000) { 
  //     Serial.println("Looping ");
  //     printMemoryUsage();
  //     lastMemoryCheck = millis();
  //   }
  // }
  // delay(1000); // Adjust the delay as needed
}

// #include <NimBLEDevice.h>
// #include <NimBLEUtils.h>
// #include <NimBLEServer.h>
// #include <NimBLEClient.h>
// #include <MyoWare.h>
// #include <vector>
// #include <ArduinoJson.h>

// // Debug parameters
// const bool debugLogging = false; // Set to true for verbose logging to serial

// std::vector<NimBLEAddress> vecMyoWareShields;
// std::vector<NimBLEClient*> vecMyoWareClients;

// // MyoWare class object
// MyoWare myoware;

// // UUIDs for the MyoWare service and characteristic
// const char *myoWareServiceUUID = "EC3AF789-2154-49F4-A9FC-BC6C88E9E930";
// const char *myoWareCharacteristicUUID = "F3A56EDF-8F1E-4533-93BF-5601B2E91308";

// // Function to read BLE data (stub for illustration)
// double ReadBLEData(BLERemoteCharacteristic* pRemoteCharacteristic) {
//   String value = pRemoteCharacteristic->readValue();  // Use String type
//   return value.toFloat();  // Convert String to float
// }

// // Function to print peripheral info (stub for illustration)
// void PrintPeripheralInfo(NimBLEClient* client) {
//   Serial.print("Peripheral: ");
//   Serial.println(client->getPeerAddress().toString().c_str());
// }

// class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
//   void onResult(BLEAdvertisedDevice* advertisedDevice) {
//     if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(myoWareServiceUUID))) {
//       if (debugLogging) {
//         Serial.printf("Found MyoWare Wireless Shield: %s \n", advertisedDevice->toString().c_str());
//       }

//       // Add device to the vector
//       vecMyoWareShields.push_back(advertisedDevice->getAddress());
//       if (debugLogging) {
//         Serial.printf("Found MyoWare Wireless Shield: %s \n", advertisedDevice->getAddress().toString().c_str());
//       }
//     }
//   }
// };

// void setup() {
//   Serial.begin(115200);
//   while (!Serial);

//   pinMode(myoware.getStatusLEDPin(), OUTPUT); // Initialize the built-in LED pin to indicate when a central is connected

//   // Begin initialization
//   NimBLEDevice::init("");  // Initialize BLE device

//   if (debugLogging) {
//     Serial.println("MyoWare BLE Central");
//     Serial.println("-------------------");
//   }

//   // Start scanning for MyoWare Wireless Shields
//   if (debugLogging) {
//     Serial.print("Scanning for MyoWare Wireless Shields: ");
//     Serial.println(myoWareServiceUUID);
//   }

//   NimBLEScan *pBLEScan = NimBLEDevice::getScan(); // Create new scan object
//   pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);  // Set scan callback
//   pBLEScan->setActiveScan(true); // Active scan uses more power, but gets results faster
//   pBLEScan->start(20, false); // Scan for 20 seconds

//   if (vecMyoWareShields.empty()) {
//     Serial.println("No MyoWare Wireless Shields found!");
//     while (1);
//   }

//   for (auto& address : vecMyoWareShields) {
//     NimBLEClient* shield = NimBLEDevice::createClient();
//     if (!shield->connect(address)) {
//       Serial.print("Failed to connect to MyoWare Wireless Shield with address: ");
//       Serial.println(address.toString().c_str());
//       continue;
//     }

//     if (debugLogging) {
//       Serial.print("Connected to ");
//       PrintPeripheralInfo(shield);
//     }

//     vecMyoWareClients.push_back(shield);
//   }

//   digitalWrite(myoware.getStatusLEDPin(), HIGH);  // Turn on the LED to indicate a connection
// }

// void loop() {
//   StaticJsonDocument<1024> doc; // Create a JSON document
//   JsonArray peripherals = doc.createNestedArray("peripherals");

//   for (auto &shield : vecMyoWareClients) {
//     JsonObject peripheral = peripherals.createNestedObject();
//     String address = shield->getPeerAddress().toString().c_str();
//     peripheral["mac"] = address;

//     if (debugLogging) {
//       Serial.print("Updating ");
//       PrintPeripheralInfo(shield);
//     }

//     if (!shield->isConnected()) {
//       peripheral["value"] = 0.0; // Output zero if the Wireless shield gets disconnected
//       continue;
//     }

//     NimBLERemoteService* myoWareService = shield->getService(myoWareServiceUUID);
//     if (!myoWareService) {
//       Serial.println("Failed finding MyoWare BLE Service!");
//       shield->disconnect();
//       peripheral["value"] = 0.0;
//       continue;
//     }

//     // Get sensor data
//     NimBLERemoteCharacteristic* sensorCharacteristic = myoWareService->getCharacteristic(myoWareCharacteristicUUID);
//     if (!sensorCharacteristic) {
//       Serial.println("Failed to find characteristic!");
//       shield->disconnect();
//       peripheral["value"] = 0.0;
//       continue;
//     }

//     const double sensorValue = ReadBLEData(sensorCharacteristic);
//     peripheral["value"] = sensorValue;
//   }

//   // Serialize JSON document to Serial
//   serializeJson(doc, Serial);
//   Serial.println(""); // Print newline to separate JSON objects
//   // delay(10); // Delay for readability and to reduce load
// }