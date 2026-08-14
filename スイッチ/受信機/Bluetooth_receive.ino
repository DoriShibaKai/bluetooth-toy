#include <Arduino.h>
#include <NimBLEDevice.h>

// ==================================================
// フォトリレー
// ==================================================
const int RELAY_PIN = 4;


// ==================================================
// Windows HTML用 独自BLE GATT
// ==================================================
#define WEB_SERVICE_UUID \
  "7b1e0001-6b7d-4a8f-9c31-123456789abc"

#define WEB_CHARACTERISTIC_UUID \
  "7b1e0002-6b7d-4a8f-9c31-123456789abc"


// ==================================================
// BLE HID
// ==================================================
static NimBLEUUID HID_SERVICE_UUID((uint16_t)0x1812);

// SpaceキーのHID Usage ID
const uint8_t HID_SPACE = 0x2C;


// ==================================================
// 状態
// ==================================================
bool webPressed = false;
bool hidPressed = false;

bool hidConnected = false;
bool doConnectHID = false;

static const NimBLEAdvertisedDevice* hidDevice = nullptr;

NimBLEServer* webServer = nullptr;


// ==================================================
// リレー更新
// ==================================================
void updateRelay() {

  if (webPressed || hidPressed) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }
}


// ==================================================
// Windows HTMLからのWrite
// ==================================================
class WebCommandCallbacks
  : public NimBLECharacteristicCallbacks {

  void onWrite(
    NimBLECharacteristic* characteristic,
    NimBLEConnInfo& connInfo
  ) override {

    (void)connInfo;

    std::string value =
      characteristic->getValue();

    String command =
      String(value.c_str());

    Serial.print("WEB received: ");
    Serial.println(command);


    if (command == "PRESS") {

      webPressed = true;

      Serial.println("WEB PRESS");
    }

    else if (command == "RELEASE") {

      webPressed = false;

      Serial.println("WEB RELEASE");
    }


    updateRelay();
  }
};


// ==================================================
// Windows側 接続・切断
// ==================================================
class WebServerCallbacks
  : public NimBLEServerCallbacks {

  void onConnect(
    NimBLEServer* server,
    NimBLEConnInfo& connInfo
  ) override {

    (void)server;
    (void)connInfo;

    Serial.println(
      "Windows/Web Bluetooth connected"
    );
  }


  void onDisconnect(
    NimBLEServer* server,
    NimBLEConnInfo& connInfo,
    int reason
  ) override {

    (void)connInfo;
    (void)reason;

    // HTML / Web Bluetoothとの接続が切れたら
    // XIAO側を含めて、すべての入力状態を強制OFFにする
    webPressed = false;
    hidPressed = false;

    // 状態に関係なく、リレーを確実にOFF
    digitalWrite(RELAY_PIN, LOW);

    Serial.println(
      "Windows/Web Bluetooth disconnected"
    );

    Serial.println(
      "ALL INPUTS OFF"
    );

    server->startAdvertising();
  }
};


WebCommandCallbacks webCommandCallbacks;
WebServerCallbacks webServerCallbacks;


// ==================================================
// XIAOからNotifyが来た時
//
// ★今回はここを診断用に強化
// ・どのUUIDから来たか
// ・何バイト来たか
// ・実際の16進データ
// を全部シリアルに表示する
// ==================================================
void hidNotifyCallback(
  NimBLERemoteCharacteristic* characteristic,
  uint8_t* data,
  size_t length,
  bool isNotify
) {

  (void)isNotify;


  Serial.println();
  Serial.println("====================");
  Serial.print("NOTIFY from UUID: ");
  Serial.println(
    characteristic->getUUID().toString().c_str()
  );

  Serial.print("Length: ");
  Serial.println(length);

  Serial.print("Data: ");


  for (size_t i = 0; i < length; i++) {

    if (data[i] < 0x10) {
      Serial.print("0");
    }

    Serial.print(data[i], HEX);
    Serial.print(" ");
  }

  Serial.println();


  // --------------------------------
  // とりあえずデータ中に
  // Space = 0x2C があるかを見る
  // --------------------------------
  bool foundSpace = false;


  for (size_t i = 0; i < length; i++) {

    if (data[i] == HID_SPACE) {

      foundSpace = true;
      break;
    }
  }


  if (foundSpace != hidPressed) {

    hidPressed = foundSpace;


    if (hidPressed) {

      Serial.println(
        "SPACE FOUND -> Relay ON"
      );

    } else {

      Serial.println(
        "SPACE RELEASE -> Relay OFF"
      );
    }


    updateRelay();
  }
}


// ==================================================
// XIAO接続・切断
// ==================================================
class HIDClientCallbacks
  : public NimBLEClientCallbacks {

  void onConnect(
    NimBLEClient* client
  ) override {

    (void)client;

    hidConnected = true;

    Serial.println(
      "XIAO Bluetooth HID connected"
    );
  }


  void onDisconnect(
    NimBLEClient* client,
    int reason
  ) override {

    (void)client;
    (void)reason;

    hidConnected = false;
    hidPressed = false;

    updateRelay();

    Serial.println(
      "XIAO Bluetooth HID disconnected"
    );

    Serial.println(
      "Scanning for XIAO again..."
    );

    NimBLEDevice::getScan()->start(
      0,
      false,
      true
    );
  }
};


HIDClientCallbacks hidClientCallbacks;


// ==================================================
// XIAOを探す
// ==================================================
class HIDScanCallbacks
  : public NimBLEScanCallbacks {

  void onResult(
    const NimBLEAdvertisedDevice* device
  ) override {

    if (!device->isAdvertisingService(
          HID_SERVICE_UUID)) {

      return;
    }


    if (device->haveName()) {

      String name =
        String(device->getName().c_str());

      if (name != "Bluetooth Switch") {

        return;
      }
    }


    Serial.println(
      "XIAO Bluetooth Switch found"
    );

    NimBLEDevice::getScan()->stop();

    hidDevice = device;

    doConnectHID = true;
  }
};


HIDScanCallbacks hidScanCallbacks;


// ==================================================
// XIAOへ接続
// ==================================================
bool connectToXIAO() {

  if (hidDevice == nullptr) {

    return false;
  }


  NimBLEClient* client =
    NimBLEDevice::createClient();


  if (client == nullptr) {

    Serial.println(
      "Could not create BLE client"
    );

    return false;
  }


  client->setClientCallbacks(
    &hidClientCallbacks,
    false
  );


  client->setConnectTimeout(5000);


  Serial.println(
    "Connecting to XIAO..."
  );


  if (!client->connect(hidDevice)) {

    Serial.println(
      "XIAO connection failed"
    );

    NimBLEDevice::deleteClient(
      client
    );

    return false;
  }


  // 暗号化完了を待つ
  Serial.println(
    "Starting secure connection..."
  );


  if (!client->secureConnection(false)) {

    Serial.println(
      "Secure connection failed"
    );
  } else {

    Serial.println(
      "Secure connection OK"
    );
  }


  // --------------------------------
  // HID Service取得
  // --------------------------------
  NimBLERemoteService* hidService =
    client->getService(
      HID_SERVICE_UUID
    );


  if (hidService == nullptr) {

    Serial.println(
      "HID service not found"
    );

    client->disconnect();

    return false;
  }


  Serial.println();
  Serial.println(
    "HID Service 0x1812 found"
  );


  // ==================================================
  // ★ここが今回の変更点
  //
  // HID Service内のCharacteristicを
  // UUIDで絞らず全部調べる
  // ==================================================
  const auto& characteristics =
    hidService->getCharacteristics(true);


  Serial.print(
    "Characteristic count: "
  );

  Serial.println(
    characteristics.size()
  );


  int subscribeCount = 0;


  for (
    NimBLERemoteCharacteristic* characteristic
      : characteristics
  ) {

    Serial.println();
    Serial.println("--------------------");

    Serial.print("UUID: ");

    Serial.println(
      characteristic->getUUID().toString().c_str()
    );


    Serial.print("Handle: 0x");

    Serial.println(
      characteristic->getHandle(),
      HEX
    );


    Serial.print("Read: ");

    Serial.println(
      characteristic->canRead()
        ? "YES"
        : "NO"
    );


    Serial.print("Write: ");

    Serial.println(
      characteristic->canWrite()
        ? "YES"
        : "NO"
    );


    Serial.print("Notify: ");

    Serial.println(
      characteristic->canNotify()
        ? "YES"
        : "NO"
    );


    Serial.print("Indicate: ");

    Serial.println(
      characteristic->canIndicate()
        ? "YES"
        : "NO"
    );


    // --------------------------------
    // NotifyできるCharacteristicなら
    // UUIDに関係なく全部購読
    // --------------------------------
    if (characteristic->canNotify()) {

      Serial.println(
        "Trying subscribe..."
      );


      if (
        characteristic->subscribe(
          true,
          hidNotifyCallback
        )
      ) {

        subscribeCount++;

        Serial.println(
          "Subscribe OK"
        );

      } else {

        Serial.println(
          "Subscribe FAILED"
        );
      }
    }
  }


  Serial.println();
  Serial.println("====================");

  Serial.print(
    "Total subscribed: "
  );

  Serial.println(
    subscribeCount
  );

  Serial.println(
    "XIAO HID DIAGNOSTIC READY"
  );

  Serial.println("====================");
  Serial.println();


  return true;
}


// ==================================================
// Windows HTML用BLEサーバー
// ==================================================
void setupWebServer() {

  webServer =
    NimBLEDevice::createServer();


  webServer->setCallbacks(
    &webServerCallbacks
  );


  NimBLEService* service =
    webServer->createService(
      WEB_SERVICE_UUID
    );


  NimBLECharacteristic* characteristic =
    service->createCharacteristic(

      WEB_CHARACTERISTIC_UUID,

      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::WRITE_NR
    );


  characteristic->setCallbacks(
    &webCommandCallbacks
  );


  service->start();


  NimBLEAdvertising* advertising =
    NimBLEDevice::getAdvertising();


  advertising->addServiceUUID(
    WEB_SERVICE_UUID
  );


  advertising->setName(
    "Bluetooth Toy Receiver"
  );


  advertising->start();


  Serial.println(
    "Windows HTML receiver ready"
  );
}


// ==================================================
// setup
// ==================================================
void setup() {

  Serial.begin(115200);

  delay(1000);


  pinMode(
    RELAY_PIN,
    OUTPUT
  );


  digitalWrite(
    RELAY_PIN,
    LOW
  );


  NimBLEDevice::init(
    "Bluetooth Toy Receiver"
  );


  NimBLEDevice::setSecurityIOCap(
    BLE_HS_IO_NO_INPUT_OUTPUT
  );


  NimBLEDevice::setSecurityAuth(
    true,
    false,
    true
  );


  setupWebServer();


  NimBLEScan* scan =
    NimBLEDevice::getScan();


  scan->setScanCallbacks(
    &hidScanCallbacks,
    false
  );


  scan->setActiveScan(true);

  scan->setInterval(100);

  scan->setWindow(80);


  Serial.println(
    "Scanning for XIAO..."
  );


  scan->start(0);
}


// ==================================================
// loop
// ==================================================
void loop() {

  if (doConnectHID) {

    doConnectHID = false;


    if (!connectToXIAO()) {

      Serial.println(
        "Retry XIAO scan"
      );


      NimBLEDevice::getScan()->start(
        0,
        false,
        true
      );
    }
  }


  delay(10);
}
