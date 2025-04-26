//Smart Plant Pot
#define BLYNK_TEMPLATE_ID "TMPL6wKNtCWZG"
#define BLYNK_TEMPLATE_NAME "Smart Plant Pot"
#define BLYNK_AUTH_TOKEN "odZkXdRoUMBsHJ2KAJF6HJUTiZUaevKn"

#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <SPI.h>
#include <Preferences.h>
#include <BluetoothSerial.h>
#include <nvs_flash.h>
#include <BH1750.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "time.h"

// BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Virtual Pins
#define VIRTUAL_TEMP V0
#define VIRTUAL_HUMID V1
#define VIRTUAL_MOIST V2
#define VIRTUAL_LED V3
#define VIRTUAL_PUMP V4
#define VIRTUAL_LIGHT V5

// Hardware Pins
#define LED_PIN 26
#define DHTPIN 5
#define DHTTYPE DHT11
#define PUMP_PIN 18
const int moisturePin = 34;

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter(0x23); // địa chỉ mặc định BH1750

//Khai báo lấy realtime
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;  // GMT+7
const int   daylightOffset_sec = 0;

// ==== BLE setup ====
BLECharacteristic *pBLECharacteristic;
std::string bleStatus = "⏳ Đang chờ dữ liệu từ BLE...";
bool newBLEDataReceived = false;
String bleReceivedData = "";
bool wifiReady = false;

class BLECallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      bleReceivedData = String(value.c_str());
      bleReceivedData.trim();
      newBLEDataReceived = true;

      pCharacteristic->setValue(bleStatus);
      pCharacteristic->notify();
    }
  }

  void onRead(BLECharacteristic *pCharacteristic) override {
    pCharacteristic->setValue(bleStatus);
  }
};

void setupBLE() {
  BLEDevice::init("SmartPlantPot");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService("12345678-1234-1234-1234-1234567890ab");

  pBLECharacteristic = pService->createCharacteristic(
    "abcdefab-1234-5678-90ab-abcdefabcdef",
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pBLECharacteristic->setCallbacks(new BLECallback());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
}

String ssid_input = "";
String password_input = "";

void setup_wifi() {
  WiFi.begin(ssid_input.c_str(), password_input.c_str());
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;
    bleStatus = "✅ Kết nối WiFi thành công!";
  } else {
    bleStatus = "❌ Kết nối WiFi thất bại!";
  }

  pBLECharacteristic->setValue(bleStatus);
  pBLECharacteristic->notify();
}

BLYNK_WRITE(VIRTUAL_PUMP) {
  int pumpState = param.asInt();
  digitalWrite(PUMP_PIN, pumpState);
}

void setup() {
  Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Khoi dong...");
  
  SPI.begin();
  setupBLE();
  setup_wifi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  dht.begin();
  Wire.begin(); // I2C
  lightMeter.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
}

void loop() {
  if (!wifiReady && newBLEDataReceived) {
    if (bleReceivedData.indexOf(',') != -1) {
      int splitIndex = bleReceivedData.indexOf(',');
      ssid_input = bleReceivedData.substring(0, splitIndex);
      password_input = bleReceivedData.substring(splitIndex + 1);
      ssid_input.trim();
      password_input.trim();

      setup_wifi();
      if (wifiReady) {
        Blynk.begin(BLYNK_AUTH_TOKEN, ssid_input.c_str(), password_input.c_str());
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi OK. Blynk on");
        delay(1000);
      }
    }
  }
  Blynk.run();

  int temperature = dht.readTemperature();
  int humidity = dht.readHumidity();
  int soilMoistureValue = analogRead(moisturePin);
  int moisture = map(soilMoistureValue, 4095, 0, 0, 100);
  float lux = lightMeter.readLightLevel();

  // Gửi dữ liệu lên Blynk
  Blynk.virtualWrite(VIRTUAL_TEMP, temperature);
  Blynk.virtualWrite(VIRTUAL_HUMID, humidity);
  Blynk.virtualWrite(VIRTUAL_MOIST, moisture);
  Blynk.virtualWrite(VIRTUAL_LIGHT, lux);

  // Điều khiển máy bơm tự động
  if (moisture <= 30) {
    digitalWrite(PUMP_PIN, HIGH);
    Blynk.virtualWrite(VIRTUAL_PUMP, 1);
  } else if (moisture > 45) {
    digitalWrite(PUMP_PIN, LOW);
    Blynk.virtualWrite(VIRTUAL_PUMP, 0);
  }

  // Điều khiển đèn LED
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed to get time");
    return;
  }

  int currentHour = timeinfo.tm_hour;
  if (currentHour >= 6 && currentHour < 18) {
    if (lux < 50) {
      digitalWrite(LED_PIN, HIGH);
      Blynk.virtualWrite(VIRTUAL_LED, 1);
    } else {
      digitalWrite(LED_PIN, LOW);
      Blynk.virtualWrite(VIRTUAL_LED, 0);
    }
  } else {
    digitalWrite(LED_PIN, LOW);  
    Blynk.virtualWrite(VIRTUAL_LED, 0);
  }

  // LCD: Hiển thị thời gian
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Thoi gian: ");
  lcd.setCursor(0, 1);
  lcd.print(timeinfo.tm_hour); lcd.print(":");
  lcd.print(timeinfo.tm_min); lcd.print(":");
  lcd.print(timeinfo.tm_sec);
  delay(2500);

  // LCD: Hiển thị nhiệt độ & độ ẩm
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhiet: "); lcd.print(temperature); lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Do am: "); lcd.print(humidity); lcd.print("%");
  delay(2500);

  // LCD: Độ ẩm đất
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Do am dat: ");
  lcd.setCursor(0, 1);
  lcd.print(moisture); lcd.print(" %");
  delay(2500);

  // LCD: Cường độ ánh sáng
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Anh sang: ");
  lcd.setCursor(0, 1);
  lcd.print((int)lux); lcd.print(" lux");
  delay(2500);

  // LCD: Trạng thái WiFi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("OK");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  } else {
    lcd.print("FAIL");
    lcd.setCursor(0, 1);
    lcd.print("Dang ket noi...");
  }
  delay(2500);

  // Serial thủ công
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '1') {
      digitalWrite(PUMP_PIN, HIGH);
      Blynk.virtualWrite(VIRTUAL_PUMP, 1);
    } else if (cmd == '0') {
      digitalWrite(PUMP_PIN, LOW);
      Blynk.virtualWrite(VIRTUAL_PUMP, 0);
    }
  }
}
