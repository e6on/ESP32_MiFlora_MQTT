/**
   A BLE client for the Xiaomi Mi Plant Sensor, pushing measurements to an MQTT server.
*/

#include "BLEDevice.h"
#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"

// variables for storing the ESP32 battery value
int ADC_Value = 0;
float voltage = 0;
float percent = 0;

// boot count used to check if battery status should be read
RTC_DATA_ATTR int bootCount = 0;

// device count
static int deviceCount = sizeof FLORA_DEVICES / sizeof FLORA_DEVICES[0];

// the remote service we wish to connect to
static BLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");

// the characteristic of the remote service we are interested in
static BLEUUID uuid_version_battery("00001a02-0000-1000-8000-00805f9b34fb");
static BLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
static BLEUUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");

// ESP32 MAC address
char macAddr[18];

TaskHandle_t hibernateTaskHandle = NULL;

WiFiClient espClient;
PubSubClient client(espClient);

void connectWifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("");
  
  byte ar[6];
  WiFi.macAddress(ar);
  sprintf(macAddr,"%02X:%02X:%02X:%02X:%02X:%02X",ar[0],ar[1],ar[2],ar[3],ar[4],ar[5]);
}

void disconnectWifi() {
  WiFi.disconnect(true);
  Serial.println("WiFi disonnected");
}

void connectMqtt() {
  Serial.println("Connecting to MQTT...");
  client.setServer(MQTT_HOST, MQTT_PORT);

  while (!client.connected()) {
    if (!client.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print("MQTT connection failed:");
      Serial.print(client.state());
      Serial.println("Retrying...");
      delay(MQTT_RETRY_WAIT);
    }
  }

  Serial.println("MQTT connected");
  Serial.println("");
}

void disconnectMqtt() {
  client.disconnect();
  Serial.println("MQTT disconnected");
}

BLEClient* getFloraClient(BLEAddress floraAddress) {
  BLEClient* floraClient = BLEDevice::createClient();
  
  if (!floraClient->connect(floraAddress)) {
    Serial.println("- Connection failed, skipping");
    return nullptr;
  }

  Serial.println("- Connection successful");
  return floraClient;
}

BLERemoteService* getFloraService(BLEClient* floraClient) {
  BLERemoteService* floraService = nullptr;

  try {
    floraService = floraClient->getService(serviceUUID);
  }
  catch (...) {
    // something went wrong
  }
  if (floraService == nullptr) {
    Serial.println("- Failed to find data service");
    Serial.println(serviceUUID.toString().c_str());
  }
  else {
    Serial.println("- Found data service");
  }

  return floraService;
}

bool forceFloraServiceDataMode(BLERemoteService* floraService) {
  BLERemoteCharacteristic* floraCharacteristic;

  // get device mode characteristic, needs to be changed to read data
  Serial.println("- Force device in data mode");
  floraCharacteristic = nullptr;
  try {
    floraCharacteristic = floraService->getCharacteristic(uuid_write_mode);
  }
  catch (...) {
    // something went wrong
  }
  if (floraCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }

  // write the magic data
  uint8_t buf[2] = {0xA0, 0x1F};
  floraCharacteristic->writeValue(buf, 2, true);

  delay(500);
  return true;
}

bool readFloraDataCharacteristic(BLERemoteService* floraService, String baseTopic) {
  BLERemoteCharacteristic* floraCharacteristic = nullptr;

  // get the main device data characteristic
  Serial.println("- Access characteristic from device");
  try {
    floraCharacteristic = floraService->getCharacteristic(uuid_sensor_data);
  }
  catch (...) {
    // something went wrong
  }
  if (floraCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }
  
  // read characteristic value
  Serial.println("- Read value from characteristic");
  std::string value;
  try {
    value = floraCharacteristic->readValue();
  }
  catch (...) {
    // something went wrong
    Serial.println("-- Failed, skipping device");
    return false;
  }
  const char *val = value.c_str();

  Serial.print("Hex: ");
  for (int i = 0; i < 16; i++) {
    Serial.print((int)val[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");

  char buffer[64];
  
  int16_t* temp_raw = (int16_t*)val;
  float temperature = (*temp_raw) / ((float)10.0);
  Serial.print("-- Temperature:  ");
  Serial.print(temperature);
  Serial.print("°C");
  if (temperature!=0 && temperature>-20 && temperature<40) {
    snprintf(buffer, 64, "%2.1f", temperature);
    if (client.publish((baseTopic + "temperature").c_str(), buffer)) {
      Serial.println("   >> Published");
    }
  }
  else {
    Serial.println("   >> Skip");
  }

  int moisture = val[7];
  Serial.print("-- Moisture:     ");
  Serial.print(moisture);
  Serial.print(" %");
  if (moisture<=100 && moisture>=0) {
    snprintf(buffer, 64, "%d", moisture);
    if (client.publish((baseTopic + "moisture").c_str(), buffer)) {
      Serial.println("   >> Published");
    }
  }
  else {
    Serial.println("   >> Skip");
  }
  
  int light = val[3] + val[4] * 256;
  Serial.print("-- Light:        ");
  Serial.print(light);
  Serial.print(" lux");
  if (light>=0) {
    snprintf(buffer, 64, "%d", light);
    if (client.publish((baseTopic + "light").c_str(), buffer)) {
      Serial.println("   >> Published");
    }
  }
  else {
    Serial.println("   >> Skip");
  }
  
  int conductivity = val[8] + val[9] * 256;
  Serial.print("-- Conductivity: ");
  Serial.print(conductivity);
  Serial.print(" uS/cm");
  if (conductivity>=0 && conductivity<5000) { 
    snprintf(buffer, 64, "%d", conductivity);
    if (client.publish((baseTopic + "conductivity").c_str(), buffer)) {
      Serial.println("   >> Published");
    }
  }
  else {
    Serial.println("   >> Skip");
  }
  
  return true;
}

bool readFloraBatteryCharacteristic(BLERemoteService* floraService, String baseTopic) {
  BLERemoteCharacteristic* floraCharacteristic = nullptr;

  // get the device battery characteristic
  Serial.println("- Access battery characteristic from device");
  try {
    floraCharacteristic = floraService->getCharacteristic(uuid_version_battery);
  }
  catch (...) {
    // something went wrong
  }
  if (floraCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping battery level");
    return false;
  }

  // read characteristic value
  Serial.println("- Read value from characteristic");
  std::string value;
  try {
    value = floraCharacteristic->readValue();
  }
  catch (...) {
    // something went wrong
    Serial.println("-- Failed, skipping battery level");
    return false;
  }
  const char *val2 = value.c_str();
  int battery = val2[0];

  char buffer[64];
  Serial.print("-- Battery:      ");
  Serial.print(battery);
  Serial.println(" %");
  snprintf(buffer, 64, "%d", battery);
  client.publish((baseTopic + "battery").c_str(), buffer);
  Serial.println("   >> Published");

  return true;
}

bool processFloraService(BLERemoteService* floraService, char* deviceMacAddress, bool readBattery) {
  // set device in data mode
  if (!forceFloraServiceDataMode(floraService)) {
    return false;
  }

  String baseTopic = MQTT_BASE_TOPIC + "/" + deviceMacAddress + "/";
  bool dataSuccess = readFloraDataCharacteristic(floraService, baseTopic);

  bool batterySuccess = true;
  if (readBattery) {
    batterySuccess = readFloraBatteryCharacteristic(floraService, baseTopic);
  }

  return dataSuccess && batterySuccess;
}

bool processFloraDevice(BLEAddress floraAddress, char* deviceMacAddress, bool getBattery, int tryCount) {
  Serial.print("Processing Flora device at ");
  Serial.print(floraAddress.toString().c_str());
  Serial.print(" (try ");
  Serial.print(tryCount);
  Serial.println(")");

  // connect to flora ble server
  BLEClient* floraClient = getFloraClient(floraAddress);
  if (floraClient == nullptr) {
    return false;
  }

  // connect data service
  BLERemoteService* floraService = getFloraService(floraClient);
  if (floraService == nullptr) {
    floraClient->disconnect();
    return false;
  }

  // process devices data
  bool success = processFloraService(floraService, deviceMacAddress, getBattery);

  // disconnect from device
  floraClient->disconnect();

  return success;
}

void hibernate() {
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000000ll);
  Serial.println("Going to sleep now.");
  delay(100);
  esp_deep_sleep_start();
}

void delayedHibernate(void *parameter) {
  delay(EMERGENCY_HIBERNATE * 1000); // delay for five minutes
  Serial.println("Something got stuck, entering emergency hibernate...");
  hibernate();
}

void setup() {
  // all action is done when device is woken up
  Serial.begin(115200);
  delay(1000);

  // increase boot count
  bootCount++;

  // create a hibernate task in case something gets stuck
  xTaskCreate(delayedHibernate, "hibernate", 4096, NULL, 1, &hibernateTaskHandle);

  Serial.println("Initialize BLE client...");
  BLEDevice::init("");
  BLEDevice::setPower(ESP_PWR_LVL_P7);

  // connecting wifi and mqtt server
  connectWifi();
  connectMqtt();

  // Reading ESP32 battery value
  ADC_Value = analogRead(ADC_Pin);
  Serial.println("");
  Serial.print("ESP32 at ");
  Serial.println(macAddr);
  Serial.print("- Battery monitoring on ADC pin ");
  Serial.println(ADC_Pin);
  Serial.print("-- Value: ");
  Serial.println(ADC_Value);
  // When you read the ADC you’ll get a 12-bit number from 0 to 4095.
  // To convert this value to a real voltage you’ll need to divide it
  // by the maximum value of 4095, then double it (note that Adafruit
  // halves the voltage), then multiply that by the reference voltage
  // of the ESP32 which is 3.3V and then finally, multiply that again
  // by the ADC Reference Voltage of 1.1V.
  voltage = ADC_Value / 4095.0 * 2.0 * 3.3 * 1.1;
  Serial.print("-- Voltage: ");
  Serial.print(voltage);
  Serial.print(" V");
  char volt[10];
  snprintf(volt, 10, "%4.2f", voltage);
  char topicv[100];
  strcpy(topicv,"esp/");
  strcat(topicv,macAddr);
  strcat(topicv,"/battery_volt");
  if (client.publish(topicv, volt)) {
    Serial.println("   >> Published");
  }
  // battery percent
  percent = ((voltage-3)/0.7)*100;
  Serial.print("-- Percent: ");
  Serial.print(percent);
  Serial.print(" %");
  char prec[10];
  snprintf(prec, 10, "%4.0f", percent);
  char topicp[100];
  strcpy(topicp,"esp/");
  strcat(topicp,macAddr);
  strcat(topicp,"/battery_prec");
  if (client.publish(topicp, prec)) {
    Serial.println("   >> Published");
  }
  Serial.println("");
  // check if battery status should be read - based on boot count
  bool readBattery = ((bootCount % BATTERY_INTERVAL) == 0);

  // process devices
  for (int i = 0; i < deviceCount; i++) {
    int tryCount = 0;
    char* deviceMacAddress = FLORA_DEVICES[i];
    BLEAddress floraAddress(deviceMacAddress);

    while (tryCount < RETRY) {
      tryCount++;
      if (processFloraDevice(floraAddress, deviceMacAddress, readBattery, tryCount)) {
        break;
      }
      delay(1000);
    }
    delay(1500);
  }

  // disconnect wifi and mqtt
  disconnectWifi();
  disconnectMqtt();

  // delete emergency hibernate task
  vTaskDelete(hibernateTaskHandle);

  // go to sleep now
  hibernate();
}

void loop() {
  /// we're not doing anything in the loop, only on device wakeup
  delay(10000);
}
