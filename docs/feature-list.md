# 機能一覧

## 対象と構成

本プロジェクトはM5StickS3とRYUW122を1ノードとして、複数TAGと複数ANCHORをESP-NOWで連携し、UWB測距を順次実行する。
現在のPlatformIO対象は`m5stack-sticks3`で、画面と電源管理はM5Unifiedを使用する。
M5Stack系へ移植する場合はM5Unifiedの共通APIを維持しつつ、board環境、表示領域、UARTのG7 TX・G1 RX割り当て、電源構成を対象機に合わせて確認する。

アプリケーションは次の層で構成する。

1. `NvsPreferenceStore`、`ConfigPreference`、`ConfigRuntime`が永続設定と実行時設定を管理する。
2. `EspNowTransport`と`EspNowBroadcast`がraw ESP-NOWとNodeStatusを管理する。
3. `TagMasterCoordinator`が最小TAG IDのmasterを選出する。
4. `NtpTimeSynchronizer`がmaster基準時刻へ同期し、現在のmaster基準時刻を提供する。
5. `Ryuw122Controller`がUART経由の非同期UWB測距を管理する。
6. `SequentialRangingProtocolCodec`と`SequentialRangingController`が順序、wire形式、逐次公開を管理する。
7. `SequentialRangingDisplay`が最新結果とhealthをM5画面へ表示する。

`main.cpp`は各クラスの生成、初期化、順番どおりの更新、画面反映に限定する。

## ノードroleと通信

- `TAG`: 測距対象。現在有効なTAGのうち最小ノードIDがmasterとなり、他はfollowerとなる。
- `ANCHOR`: RYUW122から指定TAGを測距し、結果をmasterへ送る。
- `NodeStatus`: ID、role、MAC、UWBアドレス、ANCHOR座標、master宣言、session IDを1秒間隔または状態変更時にESP-NOW broadcastする。
- node IDはroleをまたいで一意とし、重複IDは測距経路から除外する。
- NodeStatusの有効期間は30秒、初回master選出待ちは500msである。

ESP-NOWはWi-Fi Stationモードで動作し、受信callbackでは固定長情報をqueueへコピーするだけとする。
送信payloadは250バイト以内で、NTP、測距制御、1件単位の測距結果、round完了を同じtransportから送る。

## 時刻同期と逐次測距

masterは各ANCHORとfollower TAGへNTP四時刻交換を3回行い、往復遅延が最小の有効サンプルを採用する。
採用結果は`NtpSyncCommit`で全非masterノードへ送り、各ノードがmaster基準時刻へ変換できる状態になってから測距を開始する。
master変更時は旧同期、旧round、重複判定、送信待ち、表示保持を破棄し、全ANCHORと全follower TAGが新しいmasterともう一度同期してから測距を再開する。

測距順はANCHOR IDを外側、TAG IDを内側とする。
3 ANCHOR×2 TAGでは次の順で繰り返す。

```text
A1-T1 -> A1-T2 -> A2-T1 -> A2-T2 -> A3-T1 -> A3-T2
```

固定50ms slotは使用せず、RYUW122の完了後に次の組み合わせへ進む。
1件ごとにmasterへ送り、masterは直ちにアプリケーションへ公開し、follower対象の結果はそのTAGへ転送する。
最終結果の受信直後に次roundを開始する。
UWB待ちは300ms、NTP応答待ちは100msで、round timeoutはノード数から計算する。

現在実装済みなのは、各ANCHORが各TAGを測距し、結果を1件ずつmaster TAGへ集約・公開し、対象がfollower TAGの場合はそのTAGへ転送するところまでである。
master TAGはroundの受信済み組み合わせと欠損を管理するが、測距結果の永続履歴や座標計算、EKFはまだ保持・実行しない。

RYUW122はG7をUART TX、G1をUART RXとして115200bpsで使用する。
NRSTはGPIO8へ接続する。起動時はUART初期化後にNRSTをLOWで200ms保持し、GPIO8を入力へ戻してハイインピーダンス開放した後、1秒を超えて待機してからAT通信を開始する。
最初のAT疎通確認に失敗した場合だけ同じNRST復旧を1回再実行し、それでも応答しない場合は起動healthへ失敗を保持表示する。
この待機は起動時だけで、非同期測距の最短周期には固定待機を追加しない。
UART開始、NRST復旧、AT疎通確認、mode、network ID、address設定は`Ryuw122Initializer`へ集約する。
modeを書き換えた場合はRYUW122の応答停止期間を避けるため、次のATコマンドまで2秒待機する。modeが既に一致する場合は追加待機しない。
network IDは`UWB00001`、UWBアドレスはroleとnode IDから`T0000001`または`A0000010`のように生成する。

## 画面

画面には次を表示する。

- ステータスバー: node ID、mode、バッテリー残量
- 逐次状態: master・follower・ANCHOR、待機・同期・測距状態、時刻品質
- TAGの現在時刻: master基準現在秒。masterは単調時刻provider、followerは同期済みローカル現在時刻から取得し、未同期時は`NOW UNSYNC`とする
- TAGのANCHOR別最新測距: 自TAG向け結果だけを最大8件、ANCHOR ID昇順で表示する。各行は距離または失敗statusと、master基準の計測完了秒を含む
- 受信ノード: `NodeMap`の先頭3件のID、role、座標
- 起動health: RYUW122、ESP-NOW transport、NodeStatus broadcastの初期化失敗

master TAGが収集した他TAG向け結果は一覧へ入れず、follower TAGでは自TAG向け転送結果を表示する。
画面上の統一時刻は、現在のmaster基準時刻を意味する。
成功距離は値に応じてmm、m、kmへ短縮し、失敗は`FAIL`、`TIMEOUT`、`MISS`で区別する。
計測時刻は現在時刻と共通の10桁master秒moduloで表示する。
時刻品質が`Synchronized`、`PowerSaveEnabled`、`ReceiveTimestampUnavailable`なら0秒を含め有効とし、`SynchronizationExpired`、`Unsynchronized`、未知値では`@UNSYNC`を表示する。
ANCHORではTAG専用の現在時刻と距離一覧を表示しない。
表示eventの取り込み、固定長一覧保持、TAGとANCHORの表示判断、描画は`SequentialRangingDisplay`へ集約する。
M5StickS3の135×240 pixel画面では、現在時刻をY座標35、最大8 ANCHOR結果をY座標47から131、受信ノード3件をY座標155、167、179へ描画する。
ANCHOR結果行は横方向文字倍率0.9で、最大ID・距離・status・10桁秒を135 pixel幅内へ収める。

## NT-ShellとNVS preferences

USB SerialとNT-Shellは115200bpsで動作する。
`pref status`、`list`、`exists`、`get`、`set`、`remove`、`clear YES`でNVSを操作できる。
詳細な構文と型は[Preferencesコマンド仕様](preferences-commands.md)を参照する。

アプリケーションが使用する設定は次のとおりである。

| key | 型 | 既定値 | 用途 |
| --- | --- | --- | --- |
| `run_mode` | `u8` | `1`（ANCHOR） | `0`はTAG、`1`はANCHOR |
| `espnow_channel` | `u8` | `4` | ESP-NOWのWi-Fiチャンネル |
| `wifi_power_save` | `bool` | `false` | Wi-Fi Modem Sleep。既定は時刻精度優先でOFF |
| `node_id` | `u8` | `0` | 全ノードで一意にするID |
| `anchor_pos_x` | `u16` | `0` | ANCHOR X座標 |
| `anchor_pos_y` | `u16` | `0` | ANCHOR Y座標 |

設定は起動時に`ConfigRuntime`へ読み込む。
NT-Shellで永続値を変更した後は、通信やRYUW122へ確実に反映するため再起動する。
本体ボタンAのmode切替は現在の実行時値だけを変更し、NVSへ保存しない。

## 主な公開クラスとファイル

| 公開クラス | ファイル | 主な用途 |
| --- | --- | --- |
| `NvsPreferenceStore` | `include/NvsPreferenceStore.h` | 型情報付きNVSアクセス |
| `PreferenceCommands` | `include/PreferenceCommands.h` | NT-Shellの`pref`コマンド |
| `ConfigPreference`、`ConfigRuntime` | `include/ConfigPreference.h`、`include/ConfigRuntime.h` | domain設定と実行時値 |
| `EspNowTransport` | `include/EspNowTransport.h` | raw ESP-NOW、peer、固定長queue |
| `EspNowBroadcast`、`NodeStatusCodec` | `include/EspNowBroadcast.h`、`include/NodeStatus.h` | ノード検出と状態wire形式 |
| `TagMasterCoordinator` | `include/TagMasterCoordinator.h` | master選出とsession変更 |
| `NtpTimeProtocolCodec`、`NtpTimeSynchronizer` | `include/NtpTimeProtocolCodec.h`、`include/NtpTimeSynchronizer.h` | NTP packet、同期、時刻変換、現在master時刻取得 |
| `Ryuw122Initializer` | `include/Ryuw122Initializer.h` | UART開始、GPIO8 NRST復旧、AT疎通と設定 |
| `Ryuw122Controller` | `include/Ryuw122Controller.h` | 初期化後の非同期測距 |
| `SequentialRangingProtocolCodec` | `include/SequentialRangingProtocolCodec.h` | 測距packetの固定wire codec |
| `SequentialRangingController` | `include/SequentialRangingController.h` | 二重loop、逐次event、round summary |
| `SequentialRangingDisplay` | `include/SequentialRangingDisplay.h` | M5画面表示 |

## 使い方の概要

1. 各端末へ一意の`node_id`、`run_mode`、共通の`espnow_channel`を設定する。
2. ANCHORでは必要に応じて`anchor_pos_x`と`anchor_pos_y`を設定する。
3. 通常は`wifi_power_save=false`のまま再起動する。
4. TAG 2台以上とANCHOR 1台以上を起動し、最小TAG IDがmasterになることを画面で確認する。
5. 同期完了後、TAG画面の現在master時刻と自TAG向けANCHOR別最新測距を確認する。

## テスト、build、保留事項

PlatformIO native環境はT-003からT-009を分離しており、T-009はproductionの選出、NTP、protocol codec、逐次測距controllerを1つのtest binaryへ直接結合する。
3 ANCHOR×2 TAGの順序、逐次公開、時刻変換、round完了、基本timeout、master変更reset、再同期をhost上で検証する。
M5StickS3は`m5stack-sticks3`環境でclean/full buildする。
T-013通常レビュー修正時点でnative testは84件すべて成功し、M5StickS3 clean/full buildも成功している。
full buildの使用量はRAM 68,624 / 327,680バイト、Flash 1,234,447 / 3,342,336バイトである。

EKFと座標計算は未実装であり、逐次結果を将来の非同期観測入力として利用する前提である。
アプリケーションACK、複雑な再送、輻輳制御、障害時の完全自動復旧、周期的再同期も未実装である。
複数実機での無線、packet loss、queue飽和、時計ドリフト、Wi-Fi省電力差、画面視認性、NT-Shell同時操作、M5Stack系への実移植は実機検証へ保留する。
