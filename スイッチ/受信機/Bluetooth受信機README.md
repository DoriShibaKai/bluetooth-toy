# Bluetooth スイッチ受信機

BLEで受信した入力をGPIO出力へ変換する受信機です。

このプログラムは，1台のESP32系受信機で次の2種類の入力を扱います。

1. Web Bluetoothから送られる `PRESS` / `RELEASE`
2. `Bluetooth Switch` というBLE HID機器から送られるSpaceキー

どちらかがONになると，GPIO 4をHIGHにします。

## 主な機能

- Web Bluetooth用の独自BLE GATT Server
- BLE HID機器に接続するCentral / Client動作
- HID Service `0x1812` を自動検索
- Spaceキー `0x2C` を検出
- GPIO 4を出力
- Web入力とHID入力を同時に管理
- BLE HID切断後は再スキャン
- Web Bluetooth切断時は全入力を強制OFF
- シリアルモニタへBLE HIDの診断情報を表示

## 出力

```cpp
const int RELAY_PIN = 4;
```

GPIO 4をリレー／フォトリレー等の制御入力として使用します。

動作は，

```text
入力ON  → GPIO 4 HIGH
入力OFF → GPIO 4 LOW
```

です。

## Web Bluetooth側

### BLE機器名

```text
Bluetooth Toy Receiver
```

### Service UUID

```text
7b1e0001-6b7d-4a8f-9c31-123456789abc
```

### Characteristic UUID

```text
7b1e0002-6b7d-4a8f-9c31-123456789abc
```

CharacteristicはWrite / Write Without Responseに対応します。

### コマンド

Web側から次の文字列を送信します。

| コマンド | 動作 |
|---|---|
| `PRESS` | Web入力ON |
| `RELEASE` | Web入力OFF |

## BLE HID側

受信機はBLE Centralとして，次のHID機器を探します。

```text
Bluetooth Switch
```

使用する標準HID Service：

```text
0x1812
```

SpaceキーのHID Usage ID：

```text
0x2C
```

HID Service内のCharacteristicを調べ，Notify可能なCharacteristicを購読します。

受信したNotifyデータの中に `0x2C` が含まれていれば，HID入力ONと判断します。

キーReleaseのレポートを受信するとHID入力をOFFにします。

## 出力の判定

通常時は，

```cpp
if (webPressed || hidPressed) {
    digitalWrite(RELAY_PIN, HIGH);
} else {
    digitalWrite(RELAY_PIN, LOW);
}
```

としているため，

- Web BluetoothがON
- BLE HIDがON

のどちらか一方でも成立すれば出力がONになります。

## Web Bluetooth切断時の安全動作

Web Bluetoothとの接続が切れた場合は，

```cpp
webPressed = false;
hidPressed = false;
digitalWrite(RELAY_PIN, LOW);
```

として，**すべての入力状態を強制的にOFF**にします。

その後，Web Bluetooth用Advertisingを再開します。

## BLE HID切断時

`Bluetooth Switch` とのBLE接続が切れた場合は，

- `hidPressed = false`
- 出力状態を再計算
- `Bluetooth Switch` の再スキャンを開始

します。

## シリアルモニタ

ボーレート：

```text
115200
```

現在のコードは診断用として，HID Service内のCharacteristic情報やNotifyの生データを詳しく表示します。

例：

```text
NOTIFY from UUID: 0x2a4d
Length: 8
Data: 00 00 2C 00 00 00 00 00
SPACE FOUND -> Relay ON
```

キーを離した場合の例：

```text
Data: 00 00 00 00 00 00 00 00
SPACE RELEASE -> Relay OFF
```

## 必要なソフトウェア

- Arduino IDE
- ESP32 Arduino Core
- NimBLE-Arduino

コードでは次を使用しています。

```cpp
#include <NimBLEDevice.h>
```

## 書き込み

1. 受信機をパソコンへ接続します。
2. Arduino IDEで `Bluetooth_receive.ino` を開きます。
3. 使用するESP32系ボードとポートを選択します。
4. 必要なライブラリをインストールします。
5. 書き込みを実行します。
6. 必要に応じてシリアルモニタを115200 bpsで開きます。

## 起動時の流れ

```text
電源ON
↓
Web Bluetooth用GATT Server開始
↓
Bluetooth Toy ReceiverとしてAdvertising
↓
Bluetooth Switchをスキャン
↓
発見
↓
BLE HID接続
↓
Secure Connection
↓
HID Serviceを取得
↓
Notifyを購読
↓
入力待ち
```

## ファイル

```text
Bluetooth_receive.ino
```

## 注意

現在のコードにはBLE HID解析用の詳細なシリアル出力が含まれています。

完成品として使用する場合でも動作には問題ありませんが，必要に応じて診断用 `Serial.print()` を整理できます。

GPIOへ接続するリレーやフォトリレーは，使用する部品の定格と配線を確認してください。
