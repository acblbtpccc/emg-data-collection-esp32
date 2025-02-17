// #include <ArduinoBLE.h>
// #include <MyoWare.h>
// // #include <WiFi.h>
// // #include <NTPClient.h>
// // #include <WiFiUdp.h>
// // #include <TimeLib.h>

// // debug parameters
// const bool debugLogging = false;      // set to true for verbose logging
// const bool debugOutput = false;       // set to true to print output values to serial

// // config parameters
// const String localName = "MyoWareSensor-2025-02-17";  // recommend making this unique for each Wireless shield
// // const char* ssid = "boluo-wifi";
// // const char* password = "54448449";
// // const char* ntpEndPoint = "ntp1.aliyun.com";
// unsigned long writeInterval = 100; // each 100ms to send packet, 100 / 8 emg = 12.5ms, > 7.5ms(minimum ble connect interval)
// unsigned long readInterval = writeInterval / 10; // 4byte for Unix timestamp, for each data point, 2 byte for ms, 2 byte for value(0-4000), 4 data points

// MyoWare::OutputType outputType = MyoWare::ENVELOPE; // select which output to print to serial

// // MyoWare class object
// MyoWare myoware;

// // BLE Service
// BLEService myoWareService(MyoWareBLE::uuidMyoWareService.c_str());

// // BLE Muscle Sensor Characteristics
// BLECharacteristic sensorCharacteristic(MyoWareBLE::uuidMyoWareCharacteristic.c_str(), BLERead | BLENotify, 20);

// // NTP client setup
// // WiFiUDP ntpUDP;
// // NTPClient timeClient(ntpUDP, ntpEndPoint, 8 * 3600); // UTC+8 for Hong Kong
// unsigned long ntpTime;
// unsigned long bootTimeMillis;

// // void connectToWiFi() {
// //   while (WiFi.status() != WL_CONNECTED) {
// //     Serial.println("Scanning for available WiFi networks...");
// //     int n = WiFi.scanNetworks();
// //     if (n == 0) {
// //       Serial.println("No networks found.");
// //     } else {
// //       Serial.printf("%d networks found:\n", n);
// //       for (int i = 0; i < n; ++i) {
// //         Serial.printf("%d: %s (%d) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
// //                       (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "encrypted");
// //       }
// //     }

// //     Serial.println("Connecting to WiFi...");
// //     WiFi.begin(ssid, password);
// //     int wifi_attempts = 0;
// //     while (WiFi.status() != WL_CONNECTED) {
// //       // myoware.blinkStatusLED(200);  // Blink quickly during WiFi connection
// //       delay(500);
// //       Serial.print(".");
// //       wifi_attempts++;
// //       if (wifi_attempts > 20) {
// //         Serial.println("\nFailed to connect to WiFi. Restarting wifi scan");
// //         break;
// //       }
// //     }
// //   }
// // }

// // String getFormattedDateTime() {
// //   unsigned long currentMillis = millis();
// //   unsigned long elapsedMillis = currentMillis - bootTimeMillis;
// //   time_t rawTime = ntpTime + elapsedMillis / 1000;
// //   unsigned long milliseconds = elapsedMillis % 1000;

// //   // Format the date and time string with milliseconds
// //   char timeOutputBuffer[30];
// //   snprintf(timeOutputBuffer, sizeof(timeOutputBuffer), "%04d-%02d-%02d %02d:%02d:%02d.%03lu",
// //            year(rawTime), month(rawTime), day(rawTime),
// //            hour(rawTime), minute(rawTime), second(rawTime), milliseconds);
// //   return String(timeOutputBuffer);
// // }

// void setup() {
//   Serial.begin(115200);
//   while (!Serial);

//   // Connect to WiFi
//   // connectToWiFi();

//   // Start NTP client
//   // timeClient.begin();

//   // Wait until we get a valid NTP time
//   // while (true) {
//   //   timeClient.update();
//   //   ntpTime = timeClient.getEpochTime();
//   //   Serial.printf("\nGet ntpTime: ");
//   //   Serial.println(ntpTime);
//   //   if (ntpTime >= 1609459200) { // 1609459200 corresponds to 2021-01-01 00:00:00
//   //     break;
//   //   }
//   //   Serial.println("Waiting for valid NTP time...");
//   //   delay(500);
//   // }
//   // Serial.println("NTP time obtained.");
//   // Serial.println("Formatted time is: ");
//   // Serial.println(getFormattedDateTime());

//   bootTimeMillis = millis(); // Record the millis() at the time we get the NTP time

//   myoware.setConvertOutput(false);    // Set to true to convert ADC output to the amplitude of the muscle activity as it appears at the electrodes in millivolts
//   myoware.setGainPotentiometer(50.);  // Gain potentiometer resistance in kOhms
//   myoware.setENVPin(A3);              // Arduino pin connected to ENV
//   myoware.setRAWPin(A4);              // Arduino pin connected to RAW
//   myoware.setREFPin(A5);              // Arduino pin connected to REF

//   pinMode(myoware.getStatusLEDPin(), OUTPUT);  // initialize the built-in LED pin to indicate when a central is connected
//   digitalWrite(myoware.getStatusLEDPin(), HIGH);

//   // begin initialization
//   bool error = !BLE.begin();
//   if (error) {
//     Serial.println("FAILED - BLE Initialization!");
//     while (error);
//   }

//   BLE.setLocalName(localName.c_str());
//   BLE.setSupervisionTimeout(1000);
//   BLE.setAdvertisedService(myoWareService);
//   myoWareService.addCharacteristic(sensorCharacteristic);
//   BLE.addService(myoWareService);

//   // set initial values for the characteristics
//   sensorCharacteristic.writeValue("");

//   BLE.advertise();

//   // if (debugLogging) {
//   //   Serial.println("Setup Complete!!");
//   //   Serial.print(BLE.address());
//   //   Serial.print(" '");
//   //   Serial.print(localName.c_str());
//   //   Serial.print("' ");
//   //   Serial.print(myoWareService.uuid());
//   //   Serial.println();
//   //   Serial.print("Waiting to connect....");
//   // }

//   digitalWrite(myoware.getStatusLEDPin(), LOW);
// }

// void loop() {
//   static unsigned long lastReadTime = 0;
//   static unsigned long lastWriteTime = 0;
//   static uint32_t values[10] = {0};
//   static unsigned long times[10] = {0};
//   static int index = 0;

//   // wait for a BLE central
//   BLEDevice central = BLE.central();
//   if (central) {
//     // if (debugLogging) {
//     //   Serial.print("Connected to central: ");
//     //   Serial.println(central.address());
//     // }

//     digitalWrite(myoware.getStatusLEDPin(), HIGH); // turn on the LED to indicate the connection

//     while (central.connected()) {
//       unsigned long currentTime = millis();
//       if (currentTime - lastReadTime >= readInterval) {
//         // Get the sensor value and timestamp
//         values[index] = myoware.readSensorOutput(outputType);
//         times[index] = currentTime - bootTimeMillis;
//         index = (index + 1) % 10;
//         lastReadTime = currentTime;
//       }

//       if (currentTime - lastWriteTime >= writeInterval) {
//         // Convert data to binary format
//         uint8_t buffer[20];
//         unsigned long ntpMillis = ntpTime * 1000 + (currentTime - bootTimeMillis);

//         // buffer[0] = (ntpMillis >> 24) & 0xFF;
//         // buffer[1] = (ntpMillis >> 16) & 0xFF;
//         // buffer[2] = (ntpMillis >> 8) & 0xFF;
//         // buffer[3] = ntpMillis & 0xFF;

//         for (int i = 0; i < 10; ++i) {
//           // buffer[i * 10 + 4] = (times[i] >> 8) & 0xFF;  // 高8位的 time
//           // buffer[i * 4 + 5] = times[i] & 0xFF;         // 低8位的 time
//           buffer[i * 2] = (values[i] >> 8) & 0xFF; // 高8位的 value
//           buffer[i * 2 + 1] = values[i] & 0xFF;        // 低8位的 value
//         }


//         sensorCharacteristic.writeValue(buffer, 20);

//         // if (debugOutput) {
//         //   Serial.println();
//         //   Serial.print("Sent data at: ");
//         //   Serial.println(getFormattedDateTime());

//         //   Serial.println("Timestamp: ");
//         //   for (int i = 0; i < 10; ++i) {
//         //     Serial.print(times[i]);
//         //     Serial.print(" ");
//         //   }
//         //   Serial.println();

//         //   Serial.println("Data: ");
//         //   for (int i = 0; i < 10; ++i) {
//         //     Serial.print(values[i]);
//         //     Serial.print(" ");
//         //   }
//         //   Serial.println();

//         //   Serial.print("Hex: ");
//         //   for (int i = 0; i < 20; ++i) {
//         //     if (i % 2 == 0) {
//         //       Serial.println(" ");
//         //     }
//         //     Serial.print(buffer[i], HEX);
//         //     Serial.print(" ");
//         //   }
//         //   Serial.println();
//         // }

//         lastWriteTime = currentTime;
//       }
//     }

//     // when the central disconnects, turn off the LED:
//     digitalWrite(myoware.getStatusLEDPin(), LOW);

//     // if (debugLogging) {
//     //   Serial.print("Disconnected from central: ");
//     //   Serial.println(central.address());
//     // }
//   } else {
//     myoware.blinkStatusLED();
//   }
// }

// // /*
// //   MyoWare BLE Peripheral Example Code
// //   Advancer Technologies, LLC
// //   Brian Kaminski
// //   8/01/2023

// //   This example sets up a MyoWare 2.0 Wireless Shield, and then reads the ENV, RAW,
// //   and REF data from the attached MyoWare 2.0 Muscle Sensor. The MyoWare 2.0
// //   Wireless Shield (the Peripheral) sends this data to a second BLE Device
// //   (the Central) over BLE.

// //   This MyoWare 2.0 Wireless Shield, aka the "BLE Peripheral", will read the sensor's
// //   output on A3-A5 where A3 is ENV, A4 is RAW, and A5 is REF. It will then store
// //   them in a single 32-bit variable, and then update that value to the
// //   "bluetooth bulletin board". When uploading to the MyoWare 2.0 Wireless Shield
// //   make sure to select "ESP32 Dev Module" as the board definition.

// //   Note, in BLE, you have services, characteristics and values.
// //   Read more about it here:

// //   https://www.arduino.cc/reference/en/libraries/arduinoble/

// //   Note, before it begins reading the ADC and updating the data,
// //   It first sets up some BLE stuff:
// //     1. sets up as a peripheral
// //     2. sets up a service and characteristic (the data)
// //         -Note, Services and characteristics have unique 128-bit UUID,
// //         -These must match the UUIDs in the code on the central device.
// //     3. advertises itself

// //   In order for this example to work, you will need a ESP32 board,
// //   and it will need to be programmed with the provided code specific to
// //   being a central device, looking for this specific service/characteristic.

// //   Note, both the service and the characteristic get unique UUIDs.

// //   The "BLE Central", will subscribe to the MyoWare 2.0 Wireless
// //   Shield's charactieristic, read it, and parse it into 4 separate bytes,
// //   then print the values to the serial terminal.

// //   Hardware:
// //   MyoWare 2.0 Wireless Shield
// //   MyoWare 2.0 Muscle Sensor
// //   USB from BLE Device to Computer.

// //   ** For consistent BT connection follow these steps:
// //   ** 1. Reset Peripheral
// //   ** 2. Wait 5 seconds
// //   ** 3. Reset Central
// //   ** 4. Enjoy BT connection
// //   **
// //   ** ArduinoBLE does not support RE-connecting two devices.
// //   ** If you loose connection, you must follow this hardware reset sequence again.
// //   **
// //   ** ArduinoBLE does not support connecting more than four peripheral devices.

// //   This example code is in the public domain.
// // */

// // #include <ArduinoBLE.h>
// // #include <MyoWare.h>

// // const String localName = "MyoWareSensor2";  // recommend making this unique for
// //                                             // each Wireless shield (e.g. MyoWareSensor1,
// //                                             // MyoWareSensor2, ...)

// // MyoWare::OutputType outputType = MyoWare::ENVELOPE; // select which output to print to serial
// //                                                     // EMG envelope (ENVELOPE) or Raw EMG (RAW))

// // // debug parameters
// // const bool debugLogging = true;      // set to true for verbose logging
// // const bool debugOutput = true;        // set to true to print output values to serial

// // // MyoWare class object
// // MyoWare myoware;

// // // BLE Service
// // BLEService myoWareService(MyoWareBLE::uuidMyoWareService.c_str());

// // // BLE Muscle Sensor Characteristics
// // BLEStringCharacteristic sensorCharacteristic(MyoWareBLE::uuidMyoWareCharacteristic.c_str(), BLERead | BLENotify, 128);

// // void setup()
// // {
// //   Serial.begin(115200);
// //   while (!Serial);

// //   myoware.setConvertOutput(false);    // Set to true to convert ADC output to the amplitude of
// //                                       // of the muscle activity as it appears at the electrodes
// //                                       // in millivolts
// //   myoware.setGainPotentiometer(50.);  // Gain potentiometer resistance in kOhms.
// //                                       // adjust the potentiometer setting such that the
// //                                       // max muscle reading is below 3.3V then update this
// //                                       // parameter to the measured value of the potentiometer
// //   myoware.setENVPin(A3);              // Arduino pin connected to ENV (defult is A3 for Wireless Shield)
// //   myoware.setRAWPin(A4);              // Arduino pin connected to RAW (defult is A4 for Wireless Shield)
// //   myoware.setREFPin(A5);              // Arduino pin connected to REF (defult is A5 for Wireless Shield)

// //   pinMode(myoware.getStatusLEDPin(), OUTPUT);  // initialize the built-in LED pin to indicate
// //                                                // when a central is connected
// //   digitalWrite(myoware.getStatusLEDPin(), HIGH);

// //   // begin initialization
// //   bool error = !BLE.begin();
// //   if (error)
// //   {
// //     Serial.println("FAILED - BLE Initialization!");

// //     while (error);
// //   }

// //   BLE.setLocalName(localName.c_str());
// //   BLE.setSupervisionTimeout(1000);
// //   BLE.setAdvertisedService(myoWareService);
// //   myoWareService.addCharacteristic(sensorCharacteristic);
// //   BLE.addService(myoWareService);

// //   // set initial values for the characteristics
// //   sensorCharacteristic.writeValue("");

// //   BLE.advertise();

// //   if (debugLogging)
// //   {
// //     Serial.println("Setup Complete!");
// //     Serial.print(BLE.address());
// //     Serial.print(" '");
// //     Serial.print(localName.c_str());
// //     Serial.print("' ");
// //     Serial.print(myoWareService.uuid());
// //     Serial.println();
// //     Serial.print("Waiting to connect...");
// //   }

// //   digitalWrite(myoware.getStatusLEDPin(), LOW);
// // }

// // void loop()
// // {
// //   // wait for a BLE central
// //   BLEDevice central = BLE.central();
// //   if (central)
// //   {
// //     if (debugLogging)
// //     {
// //       Serial.print("Connected to central: ");
// //       Serial.println(central.address());
// //     }

// //     digitalWrite(myoware.getStatusLEDPin(), HIGH); // turn on the LED to indicate the
// //                                                    // connection

// //     while (central.connected())
// //     {
// //       // Read sensor output
// //       const String strValue = String(myoware.readSensorOutput(outputType));
// //       if (debugOutput)
// //         Serial.println(strValue.c_str());

// //       // "post" to "BLE bulletin board"
// //       sensorCharacteristic.writeValue(strValue);
// //     }

// //     // when the central disconnects, turn off the LED:
// //     digitalWrite(myoware.getStatusLEDPin(), LOW);

// //     if (debugLogging)
// //     {
// //       Serial.print("Disconnected from central: ");
// //       Serial.println(central.address());
// //     }
// //   }
// //   else
// //   {
// //     myoware.blinkStatusLED();
// //   }
// // }
