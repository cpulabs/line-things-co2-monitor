
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <bluefruit.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_CCS811.h"

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LINE Things CO2 Monitor"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID ""

#define AIR_SERVICE_UUID "7ebad564-1669-4bd7-abc8-1c9c4d2dcc26"
#define AIR_CO2_CHARACTERISTIC_UUID "a4f521de-ea92-4269-9e4f-16a8cac25178"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID ""
#define PSDI_CHARACTERISTIC_UUID ""


Adafruit_CCS811 ccs;
Adafruit_SSD1306 display = Adafruit_SSD1306();

uint8_t userServiceUUID[16];
uint8_t airServiceUUID[16];
uint8_t airCo2CharacteristicUUID[16];

uint8_t psdiServiceUUID[16];
uint8_t psdiCharacteristicUUID[16];

BLEService userService;
BLEService airService;
BLECharacteristic co2Characteristic;
BLECharacteristic ambientTempCharacteristic;
BLEService psdiService;
BLECharacteristic psdiCharacteristic;

SoftwareTimer timer;
volatile bool refreshSensorValue = false;

void setup() {
  Serial.begin(115200);

  Bluefruit.begin();
  Bluefruit.setName(DEVICE_NAME);

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");

  if(!ccs.begin()){
    Serial.println("Failed to start sensor! Please check your wiring.");
    while(1);
  }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)

  display.display();
  delay(500);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  //calibrate temperature sensor
  while(!ccs.available());
  float temp = ccs.calculateTemperature();
  ccs.setTempOffset(temp - 25.0);

  Serial.println("IO test");

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);

  timer.begin(200, triggerRefreshSensorValue);
  timer.start();

}


void triggerRefreshSensorValue(TimerHandle_t xTimer) {
  refreshSensorValue = true;
}


void loop() {
  if (refreshSensorValue) {
    if(ccs.available()){
      display.clearDisplay();
      float temp = ccs.calculateTemperature();
      display.setCursor(0,0);
      if(!ccs.readData()){
        display.print("eCO2: ");
        //Serial.print("eCO2: ");
        float eCO2 = ccs.geteCO2();
        display.print(eCO2);
        //Serial.print(eCO2);

        display.print(" ppm\nTVOC: ");
        //Serial.print(" ppm, TVOC: ");
        float TVOC = ccs.getTVOC();
        display.print(TVOC);
        //Serial.print(TVOC);

        //Serial.print(" ppb   Temp:");
        display.print(" ppb\nTemp: ");
        //Serial.println(temp);
        display.println(temp);
        display.display();

        co2Characteristic.notify16((uint16_t) eCO2);
        refreshSensorValue = false;
        Serial.println(eCO2);
      }
    }
  }
}


void setupServices(void) {
  // Convert String UUID to raw UUID bytes
  strUUID2Bytes(USER_SERVICE_UUID, userServiceUUID);
  strUUID2Bytes(AIR_SERVICE_UUID, airServiceUUID);
  strUUID2Bytes(AIR_CO2_CHARACTERISTIC_UUID, airCo2CharacteristicUUID);
  strUUID2Bytes(PSDI_SERVICE_UUID, psdiServiceUUID);
  strUUID2Bytes(PSDI_CHARACTERISTIC_UUID, psdiCharacteristicUUID);

  // Setup User Service
  userService = BLEService(userServiceUUID);
  userService.begin();

  airService = BLEService(airServiceUUID);
  airService.begin();

  co2Characteristic = BLECharacteristic(airCo2CharacteristicUUID);
  co2Characteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  co2Characteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  co2Characteristic.setFixedLen(2);
  co2Characteristic.begin();

  // Setup PSDI Service
  psdiService = BLEService(psdiServiceUUID);
  psdiService.begin();

  psdiCharacteristic = BLECharacteristic(psdiCharacteristicUUID);
  psdiCharacteristic.setProperties(CHR_PROPS_READ);
  psdiCharacteristic.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  psdiCharacteristic.setFixedLen(sizeof(uint32_t) * 2);
  psdiCharacteristic.begin();

  // Set PSDI (Product Specific Device ID) value
  uint32_t deviceAddr[] = { NRF_FICR->DEVICEADDR[0], NRF_FICR->DEVICEADDR[1] };
  psdiCharacteristic.write(deviceAddr, sizeof(deviceAddr));
}

void startAdvertising(void) {
  // Start Advertising
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(userService);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

// UUID Converter
void strUUID2Bytes(String strUUID, uint8_t binUUID[]) {
  String hexString = String(strUUID);
  hexString.replace("-", "");

  for (int i = 16; i != 0 ; i--) {
    binUUID[i - 1] = hex2c(hexString[(16 - i) * 2], hexString[((16 - i) * 2) + 1]);
  }
}

char hex2c(char c1, char c2) {
  return (nibble2c(c1) << 4) + nibble2c(c2);
}

char nibble2c(char c) {
  if ((c >= '0') && (c <= '9'))
    return c - '0';
  if ((c >= 'A') && (c <= 'F'))
    return c + 10 - 'A';
  if ((c >= 'a') && (c <= 'f'))
    return c + 10 - 'a';
  return 0;

}