# RangeAggregation

`RangeAggregation`は、複数のANCHORからTAGへUWB距離を集約する固定容量のportable coreです。特定のboard、Arduino runtime、保存媒体、console、表示、ESP-NOW実装、UWB製品libraryには依存しません。wire protocol、最大8台のANCHOR、TAGのsweep状態機械、ANCHORのcached report再送を提供します。座標計算は対象外です。

## 公開header

- `RangeAggregationTypes.h`: 公開値型、status、10 byte tokenと18 byte reportのcodec
- `IRangeTransport.h`: ESP-NOW相当の送受信eventを渡すtransport interface
- `IUwbRangeProvider.h`: blocking UWB測距を抽象化するprovider interface
- `TagRangeAggregator.h`: TAG側のpeer順次測距と完成sweep取得
- `AnchorRangeResponder.h`: ANCHOR側の測距、report送信、duplicate tokenへのcached resend

すべてのheaderはbasenameでincludeします。型とAPIはglobal namespaceにあります。transportとproviderはapplication側で実装し、`TagRangeAggregator`または`AnchorRangeResponder`のconstructorへ注入してください。TAG側は`Update(nowMs)`を継続的に呼び、`TryTakeCompletedSweep()`がtrueを返したときにapplicationが結果を利用します。library内部は出力を行いません。

## NodeMCUでの利用

このrepositoryのNodeMCU applicationは、従来どおり`#include "TagRangeAggregator.h"`などのbasename includeを使います。ESP8266 legacy ESP-NOW、EEPROM設定、NT-Shell、RYUW122、GPIO、Serial出力はapplication側の`EspNowTransport`、`RangeAggregationSetting`、`LocalShellCommands`、`UwbController`、`main.cpp`が所有します。

## M5Stackでの利用

M5Stack applicationでは、選定機種とArduino-ESP32 versionに対応したESP32 ESP-NOW transport、UART/UWB provider、storage、設定UIを別途実装します。ESP32 transportは`IRangeTransport`、UWB providerは`IUwbRangeProvider`を実装し、固定channel、STA MAC、unicast peer、callbackから固定queueへのcopy、同一MAC送信の直列化をapplication側で保証します。

対象M5Stack機種、PlatformIO board ID、`espressif32` platformおよびArduino-ESP32 versionはこのlibraryでは固定していません。adapter実装を始める前に対象機種とversionを選定してpinし、対象boardでclean buildしてください。

## Portable composition example

`examples/PortableComposition/PortableComposition.cpp`は、固定容量fake transport/providerを使い、dependency injectionと`TryTakeCompletedSweep()`の接続点を示す標準C++のcompile可能な例です。実際のESP32 adapter、保存、UI、Serial出力、座標計算は実装していません。

## 検証方法と対応状況

repository rootで次のコマンドを実行します。T-003では、portable coreだけをArduino stubなしで検証する4件が成功済みです。

```console
platformio run -e native_portable -t clean
platformio test -e native_portable --without-uploading
```

NodeMCU application、EEPROM、console、ESP8266 transport stubを含む既存native testは18件が成功済みです。

```console
platformio run -e native -t clean
platformio test -e native --without-uploading
```

NodeMCU firmwareはclean後のfull buildが成功済みです。

```console
platformio run -e nodemcuv2 -t clean
platformio run -e nodemcuv2
```

`platformio`が`PATH`にないWindows環境では、PlatformIO Coreのvirtual environmentにある`platformio.exe`を同じ引数で実行できます。たとえばPowerShellでは、標準のuser directoryへ導入済みなら`& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"`を`platformio`の代わりに使用します。

portable exampleはC++11 compilerでproduction sourceと一緒にcompileし、process exit code 0を確認します。次は`g++`が`PATH`にある環境の例です。

```console
g++ -std=c++11 -Ilib/RangeAggregation/src lib/RangeAggregation/src/RangeAggregationTypes.cpp lib/RangeAggregation/src/TagRangeAggregator.cpp lib/RangeAggregation/src/AnchorRangeResponder.cpp lib/RangeAggregation/examples/PortableComposition/PortableComposition.cpp -o .pio/build/PortableComposition
./.pio/build/PortableComposition
```

Windowsでは出力名を`.pio/build/PortableComposition.exe`として実行できます。PlatformIO同梱のMinGWを使う場合は、そのtoolchainの`bin` directoryを`PATH`へ追加してから同じcompile sourceを指定します。

これらはlocal検証結果であり、このrepositoryにはremoteとCI workflowがありません。対象M5Stack機種、PlatformIO board ID、Arduino-ESP32 version、ESP32 transport／UWB adapterは未選定・未実装です。このためM5Stack向けcompileと実機試験は未確認であり、M5Stack対応完了とは扱いません。

## APIコメント規約

libraryとexampleの全関数は、日本語のDoxygenブロックコメントで説明します。canonical declarationがheaderにある場合はheader側へ記載し、別宣言を持たない内部関数はdefinitionへ記載します。コメントには日本語の`@brief`を必須とし、引数ごとに宣言順の`@param`、非void関数に`@return`を付けます。constructor、destructor、void関数には`@return`を付けません。library内ではXML形式のfunction commentを使用しません。

## 未対応範囲

- ESP32/M5Stack向けESP-NOW adapter
- M5Stack固有のUART pin、NRST、電源、logic level
- M5Stack向けstorage、設定UI、表示
- 座標field、trilateration、least-squares
- 複数TAG、暗号化、peer discovery

M5Stack対応は、機種選定後のadapter buildとESP8266との双方向実機試験が完了するまで完了扱いにしません。
