// ================== CẤU HÌNH BLYNK ==================
#define BLYNK_TEMPLATE_ID "TMPL6gP7fXMxS"
#define BLYNK_TEMPLATE_NAME "Smart Home"
#define BLYNK_AUTH_TOKEN "lkM5hQg2XQjv_kwpEneU5fyOTvr-zHdW"

// ================== THƯ VIỆN ==================
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>         // Thư viện RFID
#include <ESP32Servo.h>
#include <BLEDevice.h>        // BLE (Bluetooth Low Energy)
#include <BLEUtils.h>
#include <BLEServer.h>
#include <DHT.h>              // Cảm biến nhiệt độ và độ ẩm

// ================== CHÂN & BIẾN ẢO TRÊN BLYNK ==================
#define VIRTUAL_TEMP V0
#define VIRTUAL_HUMID V1
#define VIRTUAL_RAIN V2
#define VIRTUAL_FLAME V3
#define VIRTUAL_GAS V4
#define VIRTUAL_DOOR V5
#define VIRTUAL_GARAGE_OPEN V6
#define VIRTUAL_GARAGE_CLOSE V7 

#define SS_PIN 5
#define RST_PIN 16
#define BUZZER_PIN 12
#define PIN_SG90 27
#define PIN_SG90_2 32
#define RAIN_SENSOR_PIN 34
#define FLAME_SENSOR_PIN 33
#define GAS_SENSOR_PIN 35
#define BUTTON1_PIN 26
#define BUTTON2_PIN 14      
#define DHTPIN 15             
#define DHTTYPE DHT11 
#define SERVO_PIN 25

// Widget Terminal cho Blynk
WidgetTerminal terminal(V8);

// ================== KHAI BÁO BIẾN BLE ==================
BLECharacteristic *pBLECharacteristic;
std::string bleStatus = "⏳ Đang chờ dữ liệu từ BLE...";
bool newBLEDataReceived = false;
String bleReceivedData = "";
bool wifiReady = false;

// ================== CLASS XỬ LÝ BLE ==================
class BLECallback : public BLECharacteristicCallbacks {
  // Xử lý khi nhận dữ liệu BLE từ điện thoại
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

  // Xử lý khi app đọc lại trạng thái từ ESP32
  void onRead(BLECharacteristic *pCharacteristic) override {
    pCharacteristic->setValue(bleStatus);
  }
};

// ================== KHỞI TẠO BLE ==================
void setupBLE() {
  BLEDevice::init("SmartHome");
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

// ================== THIẾT BỊ ==================
MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo sg90, sg90_2, sg360;
DHT dht(DHTPIN, DHTTYPE); 

// UID RFID hợp lệ
byte validUID1[4] = {0x13, 0xA2, 0x1A, 0x2D};
byte validUID2[4] = {0x5A, 0xB2, 0xB5, 0x02};

// ================== CÁC BIẾN TRẠNG THÁI ==================
bool doorOpen = false;
String ssid_input = "";
String password_input = "";
bool reverseDirection = false;
bool garageForward = false;
bool garageReverse = false;

// ================== KẾT NỐI WIFI ==================
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
    terminal.println("✅ Kết nối WiFi thành công!");
  } else {
    bleStatus = "❌ Kết nối WiFi thất bại!";
    terminal.println("❌ Kết nối WiFi thất bại!");
  }

  pBLECharacteristic->setValue(bleStatus);
  pBLECharacteristic->notify();
}

// ================== SO SÁNH UID RFID ==================
bool compareUID(byte *cardUID, byte *targetUID) {
  for (byte i = 0; i < 4; i++) {
    if (cardUID[i] != targetUID[i]) return false;
  }
  return true;
}

// ================== PHÁT ÂM THANH TỪ BUZZER ==================
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

// ================== LƯU VÀ KHÔI PHỤC TRẠNG THÁI CỬA ==================
void saveDoorState(bool state) { doorOpen = state; }

void restoreDoorState() {
  if (doorOpen) {
    sg90.write(90);
    doorOpen = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door closed");
    terminal.println("Door closed");
  }
}

// ================== KIỂM TRA THẺ RFID ==================
void checkRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    lcd.setCursor(0, 0);
    lcd.print("Scan card...     ");
    terminal.println("Scan card...     ");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    lcd.print(mfrc522.uid.uidByte[i], HEX);
    lcd.print(" ");
  }

  // Nếu thẻ hợp lệ
  if (compareUID(mfrc522.uid.uidByte, validUID1) || compareUID(mfrc522.uid.uidByte, validUID2)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Granted");
    terminal.println("Access Granted");
    Blynk.virtualWrite(VIRTUAL_DOOR, HIGH);

    beep(500);
    delay(2000);

    sg90.write(180); // mở cửa
    saveDoorState(true);
    delay(5000);     // mở trong 5s
    sg90.write(90);  // đóng lại
    saveDoorState(false);

    Blynk.virtualWrite(VIRTUAL_DOOR, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door closed");
    terminal.println("Door closed");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Denied");
    terminal.println("Access Denied");
    beep(500);
  }

  mfrc522.PICC_HaltA();
}

// ================== ĐỌC CẢM BIẾN NHIỆT ĐỘ - ĐỘ ẨM ==================
void readSensors() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) return;

  Blynk.virtualWrite(VIRTUAL_TEMP, temperature);
  Blynk.virtualWrite(VIRTUAL_HUMID, humidity);

  lcd.setCursor(0, 1);
  lcd.print("T:"); lcd.print(temperature); lcd.print("C ");
  lcd.print("H:"); lcd.print(humidity); lcd.print("%");

  terminal.print("T:"); terminal.print(temperature); terminal.print("C \n");
  terminal.print("H:"); terminal.print(humidity); terminal.print("%\n");
  terminal.flush();
  delay(500);
}

// ================== CẬP NHẬT TRẠNG THÁI SERVO GARAGE ==================
void updateGarageServo() {
  if (garageForward && !garageReverse) {
    sg360.write(180); // quay xuôi
  } else if (garageReverse && !garageForward) {
    sg360.write(0);   // quay ngược
  } else {
    sg360.write(92);  // dừng
  }
}

// ================== BLYNK - ĐIỀU KHIỂN MỞ GARAGE ==================
BLYNK_WRITE(VIRTUAL_GARAGE_OPEN) {
  garageForward = param.asInt();
  updateGarageServo();
}

BLYNK_WRITE(VIRTUAL_GARAGE_CLOSE) {
  garageReverse = param.asInt();
  updateGarageServo();
}

// ================== SETUP BAN ĐẦU ==================
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP); 

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Khoi dong...");
  terminal.println("Khoi dong...");

  SPI.begin();
  mfrc522.PCD_Init();

  sg90.attach(PIN_SG90, 500, 2400);
  sg90_2.attach(PIN_SG90_2, 500, 2400);
  sg360.attach(SERVO_PIN);
  sg90.write(90);
  sg90_2.write(45);
  sg360.write(92); // dừng ban đầu

  dht.begin(); 
  setupBLE(); // khởi động BLE
}

// ================== VÒNG LẶP CHÍNH ==================
void loop() {
  // Nếu nhận được thông tin WiFi từ app BLE thì kết nối
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
        terminal.print("WiFi OK. Blynk on");
        delay(1000);
        restoreDoorState();
      }
    }
  }

  // Nếu đã có WiFi thì thực hiện các chức năng
  if (wifiReady) {
    Blynk.run();
    readSensors();
    checkRFID();
  }

  // Xử lý cảm biến mưa
  int rainState = digitalRead(RAIN_SENSOR_PIN);
  Blynk.virtualWrite(VIRTUAL_RAIN, rainState == LOW ? 1 : 0);
  sg90_2.write(rainState == LOW ? 0 : 45);  // thu mái che

  // Xử lý cảm biến lửa
  int flameState = analogRead(FLAME_SENSOR_PIN);
  Blynk.virtualWrite(VIRTUAL_FLAME, flameState < 200 ? HIGH : LOW);

  // Xử lý cảm biến gas
  int gasState = analogRead(GAS_SENSOR_PIN);
  Blynk.virtualWrite(VIRTUAL_GAS, gasState > 800 ? HIGH : LOW);

  // Nếu phát hiện cháy hoặc gas -> mở cửa và bật còi
  bool flameDetected = (flameState <= 200);
  bool gasDetected = (gasState > 1000);
  if (flameDetected || gasDetected) {
    sg90.write(180);
    saveDoorState(true);
    Blynk.virtualWrite(VIRTUAL_DOOR, HIGH);

    while (flameDetected || gasDetected) {
      beep(1000);
      delay(300);
      flameDetected = (analogRead(FLAME_SENSOR_PIN) <= 200);
      gasDetected = (analogRead(GAS_SENSOR_PIN) > 1000);
    }

    sg90.write(90);
    saveDoorState(false);
    Blynk.virtualWrite(VIRTUAL_DOOR, LOW);
  }

  // Nút điều khiển servo 360 bằng tay
  bool currentButtonState = digitalRead(BUTTON1_PIN) == LOW;
  bool reverseButtonState = digitalRead(BUTTON2_PIN) == LOW;
  if (currentButtonState && !reverseButtonState) {
    sg360.write(180);
  } else if (reverseButtonState && !currentButtonState) {
    sg360.write(0);
  } else if (!garageForward && !garageReverse) {
    sg360.write(92); // dừng
  }

  delay(1500);
}
