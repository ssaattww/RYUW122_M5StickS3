# Sub-agent実行レポート

## タスク

- 目的: GPIO8を使用するRYUW122 NRST復旧の実装境界を調査する
- タスク種別: 実装前調査

## sub-agentを使う理由

- 理由: 外部一次資料と既存RYUW122実装を独立照合し、安全な変更境界を確定するため

## 対象範囲

- 対象: RYUW122初期化順、GPIO8 NRST制御、既存test seam、設計・機能一覧への影響

## 対象外

- 対象外: 実装、実機測定、高度な連続自動復旧、他GPIO変更

## 実行コマンド

- 実行コマンド: `Get-Content -Raw`で指定Skill・追跡・設計・test規約・production・依存RYUW122を読取り、`rg -n`でT-011/P7、GPIO8、初期化・test seam、M5Unified/M5GFXのStickS3 pin使用を照合した。`Invoke-WebRequest`で指定の[復旧実験資料](https://github.com/necobit/UWB-module-test/blob/master/docs/sleep-recovery.md)、[M5Stack公式StickS3資料](https://docs.m5stack.com/en/core/StickS3)、[Espressif公式GPIO資料](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html)を直接確認した。build、test、Git操作は実行していない。

## 対象ファイル

- 変更または確認したファイル: 変更は本レポートの5 placeholderだけ。確認対象は`tasks-status.md`のT-011、`phases-status.md`のP7、`test/README`、`include/Ryuw122Controller.h`、`src/Ryuw122Controller.cpp`、`src/main.cpp`の初期化・更新箇所、`platformio.ini`、`test/test_t005/test_main.cpp`と同test stub、`docs/sequential-ranging-time-sync.md`、`docs/feature-list.md`、`.pio/libdeps/m5stack-sticks3/RYUW122/RYUW122.h`、`RYUW122.cpp`、`includes/RYUW122_enums.h`、`README.md`、およびread-onlyのM5Unified/M5GFX StickS3 pin定義。

## 指摘事項

- 指摘要約または「指摘なし」: (1) ユーザー指定の初期化分離規則に従い、UART開始、GPIO8復旧、AT疎通、mode、network ID、address設定は`Ryuw122Initializer.h/.cpp`の1クラスへ集約する。GPIO8の物理操作だけは実機`Ryuw122HardwarePort`が`IRyuw122Port::Recover()`として提供し、実行順序と再試行判断は`Ryuw122Initializer`が一括管理する。(2) `Ryuw122Initializer::Begin()`は既存portのUART `Begin()`成功後、`Recover()`でGPIO8をOUTPUT、LOW、200ms待機、INPUTへ戻してHIGH駆動しないHigh-Z開放、1001ms以上待機し、その後に`Test()`とmode/network ID/address設定を行う。最初の`Test()`失敗時だけ`Recover()`と`Test()`を1回再実行し、最大はUART Begin 1回、Recover 2回、Test 2回とする。設定失敗・測距失敗・測距中には復旧しない。(3) 依存RYUW122 v1.0.1のreset-pin overloadはUART開始後にLOW 5ms、HIGH 5msを行うため、200ms、High-Z、1秒超待機という要件を満たさず使用できない。`.pio/libdeps`は変更しない。(4) production、test、PlatformIO設定にGPIO8の既存使用はなく、M5Stack公式StickS3 pin mapではG8はHat2-Bus pin 9に公開され、内部LCD、I2C、button、audioの割当には含まれない。ユーザー指定配線を採用できるコード上の競合は見つからない。(5) `native_t005`で`Ryuw122Initializer.cpp`もproduction sourceとしてcompileし、Arduino stubの`pinMode`、`digitalWrite`、`delay`とRYUW122 stubの`begin`、`test`を共有event logへ記録すれば、実機adapter経路のpin=8、UART begin→LOW→OUTPUT→200ms→INPUT→1001ms以上→AT testの実順序を検証できる。差し替えportでは初回成功、初回Test失敗後成功、2回失敗打切りを検証し、Recover/Test回数上限、`CommunicationFailed`、`IsReady()==false`を確認する。既存`TestHardwarePortContract`と非同期測距testはG7 TX、G1 RX、115200bps、UART command、response FIFO、timeout/late drainの回帰証拠として維持する。(6) 設計書は`Ryuw122Initializer`の責務・初期化sequence・1回限定retry・依存library reset非採用・測距中非実行を追記し、機能一覧はGPIO8 NRST配線、起動health、公開class/file表、起動限定待機を追記する。通常の`Update()`/`StartRanging()`経路へdelayやRecoverを入れないため、300ms timeoutと逐次非同期測距の最短周期には影響しない。

## 結果

- 結果: 実装seamは確定した。`Ryuw122Initializer`がUART開始、GPIO8のLOW 200ms、High-Z開放、1001ms以上待機、AT疎通、1回限定再試行、mode/network ID/address設定を一括管理する。`IRyuw122Port::Recover()`は実機・fake共通の物理復旧要求だけを表し、`Ryuw122Controller`はinitializerの結果を受けて非同期測距状態機械へ集中する。G7 TX、G1 RX、115200bpsのRYUW122生成と非同期測距経路は保持できる。

## リスク

- 未解決のリスクまたは後続対応: 指定復旧資料のUARTウェッジと復旧実測はRYUW122 firmware V1.0.21であり、対象個体・配線でGPIO8がNRSTへ正しく接続され、INPUT切替後にモジュール側pull-upで確実に解除されること、実際に起動時ウェッジから復旧することは実機確認が必要でheldとする。host testはAPI呼出順と時間値を証明するが電圧、立上り、GPIO8の実配線は証明しない。最大2回のblocking復旧待機は起動時間だけを延長するためhealth表示開始の遅延を許容するか実機で確認する。RXDフロート予防のpull-upは指定資料の別論点であり、T-011ではNRST復旧を越えて追加しない。
