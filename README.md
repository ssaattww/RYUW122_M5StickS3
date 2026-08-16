# RYUW122 + M5StickS3 UWB ranging

M5StickS3とRYUW122を1ノードとして使用し、複数のTAG/ANCHOR間でESP-NOWによるノード共有・時刻同期・順次UWB測距を行うファームウェアです。
同じファームウェアをTAGとANCHORの両方で使用し、NVSに保存した`run_mode`と`node_id`で役割を決定します。

詳細な通信設計とシーケンス図は[複数TAG順次測距・マスターTAG基準時刻同期設計](docs/sequential-ranging-time-sync.md#14-シーケンス図)を参照してください。

## M5StickS3とRYUW122の結線

この結線は6ピンの`RYUW122_Lite`評価ボードを前提とします。
RYUW122本体モジュールを直接使用する場合はピン番号が異なるため、RYUW122本体のデータシートに従ってください。

| M5StickS3 Hat2-Bus | RYUW122_Lite | 用途 |
| --- | --- | --- |
| `3V3_L2`（13番） | `VDD`（1番） | 3.3V電源 |
| `GND`（1番） | `GND`（6番） | GND |
| `G7`（8番） | `RXD`（3番） | M5StickS3 TX -> RYUW122 RX |
| `G1`（7番） | `TXD`（4番） | M5StickS3 RX <- RYUW122 TX |
| `G8`（9番） | `NRST`（2番） | RYUW122リセット |
| 未接続 | `PA7`（5番） | RYUW122のNormal/Sleep表示。本ファームウェアでは未使用 |

RYUW122のVDD仕様は2.4Vから3.6V、標準3.3Vです。`EXT_5V`やGroveの5VをRYUW122のVDDへ接続しないでください。
UARTは115200bpsで使用します。

起動時、ファームウェアは`G8`でRYUW122をリセットした後、NVS設定に合わせてRYUW122を自動設定します。
RYUW122へ直接ATコマンドを入力してmode、network ID、addressを設定する必要はありません。

- network ID: `UWB00001`
- TAG address: `T` + 7桁のnode ID。例: node ID 1 -> `T0000001`
- ANCHOR address: `A` + 7桁のnode ID。例: node ID 10 -> `A0000010`

## NT-Shellで初期設定する

通常の`m5stack-sticks3`ビルドではNT-Shellが常時有効です。特別な「設定モード」への切り替えは不要です。
USBシリアルを115200bpsで開くと`> `プロンプトを使用できます。画面上部に`SH`が表示されていればNT-Shell有効ビルドです。

```text
> help
help         Show command list
pref         Read and write NVS preferences
```

`pref help`でPreferencesコマンドの使用方法を確認できます。

```text
pref status
pref list
pref exists <key>
pref get <type> <key>
pref set <type> <key> <value>
pref remove <key>
pref clear YES
```

詳細は[Preferencesコマンド仕様](docs/preferences-commands.md)を参照してください。

### 必要な設定

| 設定 | 型 | 対象 | 内容 | 初期値 |
| --- | --- | --- | --- | --- |
| `node_id` | `u8` | 全ノード | ノードID。TAG/ANCHORを通して一意にする | `0` |
| `run_mode` | `u8` | 全ノード | `0`: TAG、`1`: ANCHOR | `1`（ANCHOR） |
| `anchor_pos_x` | `u16` | ANCHOR | ANCHORのX座標 [mm] | `0` |
| `anchor_pos_y` | `u16` | ANCHOR | ANCHORのY座標 [mm] | `0` |

`anchor_pos_x`と`anchor_pos_y`は0から65535 mmの範囲です。負の座標が必要な配置では、すべてのANCHOR座標が非負になるよう原点をずらしてください。
TAGではANCHOR座標は自己位置計算に使用しません。

### TAGの設定例

node ID 1のTAGにする例です。

```text
pref set u8 node_id 1
pref set u8 run_mode 0
pref list
```

### ANCHORの設定例

node ID 10、座標 `(0, 0)` mmのANCHORにする例です。

```text
pref set u8 node_id 10
pref set u8 run_mode 1
pref set u16 anchor_pos_x 0
pref set u16 anchor_pos_y 0
pref list
```

node ID 11、座標 `(3000, 0)` mmの場合は次のように設定します。

```text
pref set u8 node_id 11
pref set u8 run_mode 1
pref set u16 anchor_pos_x 3000
pref set u16 anchor_pos_y 0
pref list
```

設定はNVSへ保存されますが、実行中の設定値には即時反映されません。設定後にM5StickS3を再起動してください。
再起動時にRYUW122のmodeとaddressもNVS設定へ合わせて自動設定されます。

### ESP-NOW設定

全ノードは同じESP-NOWチャンネルを使用する必要があります。初期値は4なので、変更しない場合は設定不要です。
変更する場合は全ノードへ同じ値を設定して再起動します。

```text
pref set u8 espnow_channel 4
```

Wi-Fi省電力は初期値`false`です。

```text
pref set bool wifi_power_save false
```

## 動作の流れ

1. NVSから`run_mode`、`node_id`、ESP-NOW設定、ANCHOR座標を読み込みます。
2. RYUW122をリセットし、UART、mode、network ID、UWB addressを初期化します。
3. ESP-NOWを開始し、各ノードが`NodeStatus`をbroadcastします。
4. 有効なTAGのうち最小node IDのTAGをmaster TAGとして選出します。
5. master TAGが各ANCHORとfollower TAGに対してNTP四時刻同期を行います。各対象について3サンプル取得し、往復時間が最小の有効サンプルを採用します。
6. 同期完了後、master TAGが逐次測距を開始します。ANCHOR IDを外側、TAG IDを内側として順番に処理します。
7. ANCHORは指定TAGをRYUW122で測距し、結果をmaster TAGへ送信します。対象がfollower TAGの場合はmaster TAGから対象TAGへ結果を転送します。
8. 各TAGは自TAG向けの結果だけを画面表示と自己位置計算に使用します。

例えばANCHORがA1、A2、A3、TAGがT1、T2の場合は次の順番です。

```text
A1-T1 -> A1-T2 -> A2-T1 -> A2-T2 -> A3-T1 -> A3-T2 -> 次round
```

固定50msスロットは使用せず、1件の測距と必要な転送が完了すると次の組み合わせへ進みます。
`NodeStatus`は通常1秒間隔でbroadcastされ、状態が変化した場合は即時送信されます。測距経路やmaster選出では最終受信から30秒以内のノードを有効として扱います。

詳細は[シーケンス図](docs/sequential-ranging-time-sync.md#14-シーケンス図)を参照してください。

## 画面の見方

### ステータスバー

画面上部には次の情報を表示します。

```text
ID:1 TAG SH                         82%
```

- `ID:1`: 自ノードの`node_id`
- `TAG` / `ANCHOR`: 自ノードの`run_mode`
- `SH`: NT-Shell有効ビルド
- `82%`: M5StickS3のバッテリー残量

### 逐次測距ステータス

測距一覧ページの先頭行は次の形式です。

```text
SEQ M RUN Q:SYNC
```

最初の1文字は現在の役割です。

| 表示 | 意味 |
| --- | --- |
| `M` | master TAG |
| `F` | follower TAG |
| `A` | ANCHOR |
| `?` | master TAG未決定 |

中央は逐次測距状態です。

| 表示 | 意味 |
| --- | --- |
| `WAIT` | master TAGの決定待ち |
| `FOLLOW` | follower TAGとして動作中 |
| `SYNC` | 時刻同期完了待ち |
| `READY` | master TAGが次roundを開始可能 |
| `RUN` | master TAGがround実行中 |
| `IDLE` | ANCHORが測距指示待ち |
| `RANGE` | ANCHORがRYUW122で測距中 |

`Q:`は測距結果に付与された時刻品質です。

| 表示 | 意味 |
| --- | --- |
| `SYNC` | 同期済み |
| `PWR` | Wi-Fi省電力有効 |
| `RX?` | ESP-NOW受信timestampを取得できない |
| `OLD` | 同期情報が期限切れ |
| `UNSYNC` | 有効な同期情報なし |

### TAGの測距一覧ページ

TAGでは通常、次の情報を表示します。

```text
SEQ M RUN Q:SYNC
NOW 000123s
OK LAST
A10 1542mm
A11 2103mm
A12 1870mm
...
CURRENT FAIL
A13 TIME 300ms
...
ID MODE X,Y
10 A 0,0
11 A 3000,0
12 A 0,3000
```

- `NOW`: 現在のmaster TAG基準時刻。同期できていない場合は`NOW UNSYNC`
- `OK LAST`: ANCHORごとの直近の成功距離。自TAG向け結果だけを最大5件、ANCHOR ID昇順で表示
- `CURRENT FAIL`: 現在失敗しているANCHOR。`FAIL`または`TIME`と測距所要時間を表示
- 失敗が発生しても`OK LAST`の直前成功値は消しません。同じANCHORの次回成功時に`CURRENT FAIL`から除外します

master TAGであっても、他TAG向けの測距結果は自TAGの`OK LAST`や自己位置計算には使用しません。
follower TAGはmaster TAGから転送された自TAG向け結果を使用します。

### TAGの自己位置ページとAボタン

TAG modeではAボタンを押すたびに、次の2ページを切り替えます。

1. 測距一覧ページ
2. 自己位置グラフページ

ANCHOR modeではAボタンによるページ切り替えは行いません。

自己位置ページでは次のように表示します。

```text
TAG POSITION A:LIST
X:1234mm
Y:987mm
A:4 RMS:63mm
```

- `X`, `Y`: 推定したTAG座標 [mm]
- `A`: 位置計算に使用したANCHOR数
- `RMS`: 推定位置から各ANCHORまでの幾何距離と実測距離の残差RMS [mm]
- 白い点: ANCHOR位置
- 赤い点: 推定TAG位置
- グラフは現在のANCHOR/TAG座標範囲へ自動スケールし、Y軸は上方向が正
- `A:LIST`: Aボタンで測距一覧へ戻ることを示す

自己位置は3台以上の有効なANCHORから線形最小二乗法で求めます。4台以上も同じ計算で使用できます。
ただし現在の画面snapshotは成功測距と受信ノードをそれぞれ最大5件保持するため、画面上の自己位置計算に使用できるANCHORは最大5台です。

位置を計算できない場合は次の表示になります。

| 表示 | 意味 |
| --- | --- |
| `NEED 3 ANCHORS` / `FOUND:n` | 有効なANCHORが3台未満 |
| `GEOMETRY ERROR` / `ANCHORS:n` | ANCHORが一直線またはほぼ一直線で位置を一意に決められない |
| `POSITION INVALID` | 入力値が位置計算に使用できない |

### TAG/ANCHOR共通のbroadcast一覧

測距一覧ページ下部のbroadcast一覧はTAG/ANCHORで共通です。

```text
ID MODE X,Y
10 A 0,0
11 A 3000,0
1 T 0,0
```

各行は受信した`NodeStatus`です。

- `ID`: 相手ノードの`node_id`
- `MODE`: `T` = TAG、`A` = ANCHOR
- `X,Y`: 相手ノードがbroadcastした`anchor_pos_x`,`anchor_pos_y` [mm]
- TAGの`X,Y`は自己位置計算には使用しません
- 画面には受信`NodeMap`の先頭5件まで表示します

この一覧は画面用の受信ノード一覧です。master選出や測距経路では、最終`NodeStatus`受信から30秒以内か、node IDが重複していないかなどを別途判定します。

## エラー・起動ステータス

RYUW122またはESP-NOWの初期化に失敗した場合は、通常画面より優先して`INIT FAILED`を表示します。

RYUW122の表示は次のとおりです。

| 表示 | 意味 |
| --- | --- |
| `RYUW122: SERIAL` | UART初期化失敗 |
| `RYUW122: AT` | RYUW122とのAT疎通失敗 |
| `RYUW122: MODE_READ` | mode読み出し失敗 |
| `RYUW122: MODE_WRITE` | mode設定失敗 |
| `RYUW122: NETWORK_READ` | network ID読み出し失敗 |
| `RYUW122: NETWORK_WRITE` | network ID設定失敗 |
| `RYUW122: ADDRESS_READ` | UWB address読み出し失敗 |
| `RYUW122: ADDRESS_WRITE` | UWB address設定失敗 |
| `RYUW122: TAG_SEND` | TAG応答payload設定失敗 |

ESP-NOW側では次の表示があります。

```text
ESP-NOW transport failed
ESP-NOW broadcast failed
```

FreeRTOSの測距/画面タスクやtask間queueを開始できない場合は次を表示します。

```text
TASK START FAILED
```

## 関連ドキュメント

- [機能一覧](docs/feature-list.md)
- [現在のクラス構成](docs/current-class-architecture.md)
- [Preferencesコマンド仕様](docs/preferences-commands.md)
- [複数TAG順次測距・時刻同期設計 / シーケンス図](docs/sequential-ranging-time-sync.md#14-シーケンス図)
- [REYAX RYUW122_Lite Datasheet](https://reyax.com/upload/products_download/download_file/RYUW122_Lite.pdf)
- [REYAX RYUW122 Datasheet](https://reyax.com/upload/products_download/download_file/RYUW122_EN.pdf)
- [M5Stack StickS3 hardware documentation](https://docs.m5stack.com/ja/core/StickS3)
