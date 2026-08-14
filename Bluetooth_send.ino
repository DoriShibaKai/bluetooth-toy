#include <bluefruit.h>

// 3.5mmジャックのTipを接続するピン
constexpr uint8_t SWITCH_PIN = D9;

// チャタリング除去時間
constexpr unsigned long DEBOUNCE_MS = 30;

// Bluetooth HID
BLEDis bledis;
BLEHidAdafruit blehid;

bool lastRawState = HIGH;
bool stableState = HIGH;
unsigned long lastChangeTime = 0;

void startAdvertising();

// 内蔵RGB LEDをすべて消す
void ledOff() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

// 赤点灯：電源ON・接続待ち
void ledRed() {
  ledOff();
  digitalWrite(LED_RED, LOW);
}

// 緑点灯：Bluetooth接続済み
void ledGreen() {
  ledOff();
  digitalWrite(LED_GREEN, LOW);
}

// 青点灯：スイッチ押下中
void ledBlue() {
  ledOff();
  digitalWrite(LED_BLUE, LOW);
}

void connectCallback(uint16_t connHandle) {
  (void)connHandle;
  ledGreen();
}

void disconnectCallback(uint16_t connHandle, uint8_t reason) {
  (void)connHandle;
  (void)reason;
  ledRed();
}

void setup() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // 内蔵RGB LED
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  // 電源が入り，プログラムが動き始めたことを表示
  ledRed();

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Bluetooth Switch");

  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

  bledis.setManufacturer("Custom Assistive Device");
  bledis.setModel("XIAO nRF52840 Plus");
  bledis.begin();

  blehid.begin();

  startAdvertising();
}

void loop() {
  const bool rawState = digitalRead(SWITCH_PIN);

  if (rawState != lastRawState) {
    lastRawState = rawState;
    lastChangeTime = millis();
  }

  if (millis() - lastChangeTime >= DEBOUNCE_MS) {
    if (rawState != stableState) {
      stableState = rawState;

      if (Bluefruit.connected()) {
        if (stableState == LOW) {
          // D9とGNDがつながった
          ledBlue();
          blehid.keyPress(' ');
        } else {
          // D9とGNDが離れた
          blehid.keyRelease();
          ledGreen();
        }
      }
    }
  }

  delay(5);
}

void startAdvertising() {
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();

  Bluefruit.Advertising.addFlags(
    BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE
  );

  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(
    BLE_APPEARANCE_HID_KEYBOARD
  );

  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}