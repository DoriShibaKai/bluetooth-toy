##  Bluetooth スイッチ送信機

外部スイッチを **Bluetooth Low Energy（BLE）HIDキーボード**として使うための送信機です。

XIAO nRF52840 Plusに3.5 mmスイッチ入力を接続し，スイッチを押すと **Spaceキー** を送信します。

## 主な機能

- BLE HIDキーボードとして動作
- BLE機器名：`Bluetooth Switch`
- 外部スイッチ入力：`D9`
- `INPUT_PULLUP` を使用
- スイッチON時にSpaceキーを送信
- スイッチOFF時にキーをRelease
- 30 msのチャタリング除去
- 内蔵RGB LEDで状態表示
  - 赤：電源ON／Bluetooth接続待ち
  - 緑：Bluetooth接続済み
  - 青：スイッチ押下中
- Bluetooth切断後は自動的にAdvertisingを再開

## 使用ハードウェア

- Seeed Studio XIAO nRF52840 Plus
- 3.5 mmモノラルジャック
- 外部スイッチ
- USB電源または対応する電源

## 配線

3.5 mmモノラルジャックを次のように接続します。

| 3.5 mmジャック | XIAO |
|---|---|
| Tip | D9 |
| Sleeve | GND |

プログラムでは `INPUT_PULLUP` を使用しているため，

- D9とGNDがつながる → スイッチON
- D9とGNDが離れる → スイッチOFF

として判定します。

## 必要なソフトウェア

- Arduino IDE
- XIAO nRF52840 Plusで使用できるnRF52 Arduino環境
- `bluefruit.h` を含むBluefruitライブラリ

## 書き込み

1. XIAO nRF52840 Plusをパソコンへ接続します。
2. Arduino IDEで `Bluetooth_send.ino` を開きます。
3. 使用するボードとポートを選択します。
4. 書き込みを実行します。

## 使い方

1. 送信機の電源を入れます。
2. LEDが赤く点灯し，Bluetooth接続を待ちます。
3. 接続先の機器から `Bluetooth Switch` を選択します。
4. 接続するとLEDが緑になります。
5. 外部スイッチを押すとLEDが青になり，Spaceキーを送信します。
6. スイッチを離すとキーをReleaseし，LEDが緑に戻ります。

## Spaceキー送信について

送信には次の処理を使用しています。

```cpp
blehid.keyPress(' ');
```

ここではHID Usage IDを直接渡すのではなく，**スペース文字 `' '`** を `BLEHidAdafruit::keyPress()` に渡しています。

スイッチを離したときは，

```cpp
blehid.keyRelease();
```

でキーを解除します。

## BLE設定

デバイス名：

```text
Bluetooth Switch
```

送信出力：

```cpp
Bluefruit.setTxPower(4);
```

AdvertisingにはHIDキーボードとしてのAppearanceとHID Serviceを登録しています。

## ファイル

```text
Bluetooth_send.ino
```

## 注意

このプログラムは外部スイッチをBLE HID入力として使用するためのサンプルです。

接続先のOSやアプリによって，Spaceキーを受け取ったときの動作は異なります。
