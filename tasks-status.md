# 複数TAG順次測距 タスク状況

このファイルは`task-breakdown-planner`、`task-consistency-manager`、`progress-sync-manager`を通じてのみ更新する。

## 実行規則

- 一度に実行するタスクは1件とする。
- 各実装タスクは、対象の検証と`tasks-status.md`の進捗更新を完了してから個別にコミットする。
- コミットは原則として1タスクにつき1件とし、別タスクの変更を混在させない。
- 実装とレビューのsub-agentは、ユーザー指定どおり`gpt-5.6-sol`、reasoning effort `high`を使用する。
- 各実装・検証・レビューは通常どおり`reports/`へレポートを作成し、該当タスクのコミットへ含める。
- すべての追加・変更関数へ日本語Doxygenコメントを付ける。
- enum class名は`En`で始め、クラス・関数名はUpperCamelCase、メンバー変数は`m_`に続くlowerCamelCaseとする。
- 基本ファイル名は主要クラス名と一致させる。
- 初期実装は正常系を優先し、アプリケーションACK、複雑な再送、輻輳制御、障害時の完全自動復旧を実装しない。
- ホストテストは`test/README`に従ってPlatformIO Test Runnerから実行し、PowerShellなど特定OS専用スクリプトへ依存させない。
- 実装対象は`C:\Users\taiga\Documents\PlatformIO\Projects\RYUW122_M5StickS3`とし、`.pio/libdeps`は編集しない。

## 現在位置

- 現在フェーズ: なし
- 完了タスク: T-001, T-002, T-003, T-004, T-005, T-006, T-007, T-008, T-009, T-010, T-011, T-012, T-013
- 次タスク: なし
- 次タスク状態: 全タスク完了
- ブランチ: `codex/display-three-nodes-tag-results`

## タスク一覧

| ID | フェーズ | タスク | 見積 | 依存 | 状態 | コミット |
| --- | --- | --- | --- | --- | --- | --- |
| T-001 | P1 | 設計確定とタスク分解 | M | なし | 完了 | `75bb8af` + 本追跡コミット |
| T-002 | P2 | ESP-NOW transportとWi-Fi設定基盤 | L | T-001 | 完了 | `b2fbdc8` |
| T-003 | P2 | NodeStatus拡張とマスターTAG選出 | M | T-002 | 完了 | `50002aa` |
| T-004 | P3 | NTP四時刻同期とマスター時刻変換 | L | T-003 | 完了 | `933901e` |
| T-005 | P3 | RYUW122非同期測距API | L | T-001 | 完了 | `2c8b156` |
| T-006 | P4 | 複数TAG測距プロトコルとcodec | M | T-003, T-004 | 完了 | `5d18194` |
| T-007 | P4 | 最短周期の順次測距状態機械 | XL | T-004, T-005, T-006 | 完了 | `c28461e` + `7771fb1` |
| T-008 | P5 | アプリケーション統合と逐次表示 | L | T-007 | 完了 | `6ce3365` |
| T-009 | P5 | 統合テスト、M5StickS3 build、文書同期 | L | T-008 | 完了 | `ecd6e37` |
| T-010 | P6 | sol high最終レビューと必要修正 | M | T-009 | 完了 | 本タスクのコミット |
| T-011 | P7 | RYUW122 GPIO8リセット復旧 | M | T-010 | 完了 | 本タスクのコミット |
| T-012 | P8 | 計測開始シーケンスと実装範囲の明文化 | S | T-011 | 完了 | 本タスクのコミット |
| T-013 | P9 | 接続先3件とTAG全ANCHOR測距結果・統一時刻の画面表示 | M | T-012 | 完了 | 本タスクのコミット |

## タスク詳細

### T-001 設計確定とタスク分解

変更対象:

- `docs/sequential-ranging-time-sync.md`
- `tasks-status.md`
- `phases-status.md`

実施内容:

- 最小TAG IDをマスターとする複数TAG設計を確定する。
- `A1-T1, A1-T2, A2-T1, A2-T2...`の二重ループ順序を定義する。
- 固定スロットを設けず、1件ごとに結果を公開する方針を定義する。
- 正常系、NTP同期、マスター交代のシーケンス図を作成する。
- 実装タスク、依存関係、完了条件、コミット境界を作成する。

完了条件:

- 設計書が複数TAG、逐次公開、NTP四時刻同期、Wi-Fi省電力設定を一貫して説明している。
- 全タスクに依存関係、見積、完了条件がある。
- Markdownの構造検査と`git diff --check`が成功する。
- 設計・追跡ファイルだけのコミットを作成する。

### T-002 ESP-NOW transportとWi-Fi設定基盤

変更対象:

- `include/EspNowTransport.h`
- `src/EspNowTransport.cpp`
- `include/ConfigPreference.h`
- `src/ConfigPreference.cpp`
- 必要最小限のbuild設定とテスト
- `reports/T-002-espnow-transport-implementation.md`
- `reports/T-002-espnow-transport-normal-review.md`

実施内容:

- raw ESP-NOWの初期化、peer管理、受信キュー、送信完了キューを実装する。
- `const esp_now_recv_info_t*`からMAC、RSSI、channel、受信timestampを固定長構造体へコピーする。
- 1件in-flightの単純な固定長FIFO送信を実装する。
- NVSの`wifi_power_save`をbool、既定値falseとして追加する。
- OFF時は`WIFI_PS_NONE`、ON時は`WIFI_PS_MIN_MODEM`をESP-NOW開始前に適用する。

完了条件:

- callback内に動的確保、画面出力、Serial出力、プロトコル処理がない。
- `.pio/libdeps`を編集していない。
- Wi-Fi省電力の既定値と設定分岐を検証できる。
- M5StickS3 clean buildが成功する。
- T-002だけのコミットを作成する。

完了結果:

- M5StickS3 clean/full build成功
- RAM 51,168 / 327,680 bytes（15.6%）
- Flash 1,215,463 / 3,342,336 bytes（36.4%）
- 通常レビュー`pass_with_held`、必須修正findingなし
- 実機ESP-NOW通信、queue飽和、timestamp折り返し、省電力差は保留

### T-003 NodeStatus拡張とマスターTAG選出

変更対象:

- `include/EspNowBroadcast.h`
- `src/EspNowBroadcast.cpp`
- `include/TagMasterCoordinator.h`
- `src/TagMasterCoordinator.cpp`
- 関連テスト

実施内容:

- `EspNowBroadcast`を`EspNowTransport`共有方式へ移行する。
- `NodeStatus`へUWBアドレス、マスター宣言、セッション情報を追加する。
- 受信データを既存の`m_nodes`へ保存する。
- 全ノードIDが一意という前提で、有効TAGの最小IDをマスターに選出する。
- より小さいIDのTAG参加、マスター消失、起動時選出を扱う。

完了条件:

- 1台および複数TAGで選出結果が決定的である。
- フォロワーTAGはラウンドを開始しない。
- マスター変更イベントで旧セッションを無効化できる。
- 既存の受信ノード一覧表示が`m_nodes`の件数分動作する。
- M5StickS3 buildと選出ロジックのホストテストが成功する。
- T-003だけのコミットを作成する。

通常レビューfinding:

- T003-NR-001 Medium: `EspNowBroadcast`が共有受信FIFOの非NodeStatus packetを破棄しない配送境界へ修正する。
- T003-NR-002 Medium: remote TAGをmaster宣言・非0 session受信前に有効masterとして公開しない。

完了結果:

- T003-NR-001 Medium、T003-NR-002 Mediumはいずれもresolved
- PlatformIO native test 5/5成功
- M5StickS3 clean/full build成功
- RAM 50,056 / 327,680 bytes（15.3%）
- Flash 1,210,243 / 3,342,336 bytes（36.2%）
- fix verification`pass_with_held`、新規findingなし
- 実機ESP-NOW、異種packet共存、複数実機マスター交代は後続へ保留

### T-004 NTP四時刻同期とマスター時刻変換

変更対象:

- `include/NtpTimeSynchronizer.h`
- `src/NtpTimeSynchronizer.cpp`
- 同期packet定義と関連テスト

実施内容:

- マスターTAGとANCHOR、フォロワーTAG間のNTP四時刻交換を実装する。
- 各ノード3サンプルから最小RTTを採用する。
- 32bit timestamp折り返しを考慮し、マスターTAGの64bit時刻へ変換する。
- フォロワーTAGへoffset、RTT、時刻品質を通知する。
- マスター変更時に全同期状態を破棄する。

完了条件:

- offset、RTT、時刻変換、折り返し、最小RTT選択をホストテストできる。
- session、sequence、対象ノードが一致しない応答を拒否する。
- Wi-Fi省電力ONと受信timestamp欠落を時刻品質へ反映する。
- M5StickS3 buildが成功する。
- T-004だけのコミットを作成する。

通常レビューfinding:

- T004-NR-001 Medium: フォロワーの時刻変換と同期経過時間を、同じマスター時計domainの移動参照で計算する。
- T004-NR-002 Medium: 同一マスターセッション中に後から有効になった未処理ノードを同期対象へ追加する。

完了結果:

- T004-NR-001 Medium、T004-NR-002 Mediumはいずれもresolved
- PlatformIO native T-004 test 13/13成功、T-003回帰test 5/5成功
- M5StickS3 clean/full build成功
- RAM 52,144 / 327,680 bytes（15.9%）
- Flash 1,215,867 / 3,342,336 bytes（36.4%）
- fix verification`pass_with_held`、新規findingなし
- 実機受信timestamp、packet loss、clock drift、Wi-Fi省電力差は後続へ保留

### T-005 RYUW122非同期測距API

変更対象:

- `include/Ryuw122Controller.h`
- `src/Ryuw122Controller.cpp`
- 関連テストまたは差し替え可能なinterface

実施内容:

- G7をTX、G1をRX、115200bpsとして初期化する既存処理を保持する。
- ANCHORから指定TAGへの非同期測距開始、更新、結果取得APIを実装する。
- 距離、UWB RSSI、開始時刻、完了時刻、300ms timeoutを返す。
- timeout後の遅延応答を次測距へ誤帰属させない。
- 正常系へ固定`delay()`を追加しない。

完了条件:

- 状態機械をブロッキングせずに更新できる。
- 成功、失敗、timeout、遅延応答を再現可能な形で検証する。
- G1/G7と115200bpsがbuild結果へ反映される。
- M5StickS3 buildが成功する。
- T-005だけのコミットを作成する。

通常レビューfinding:

- T005-NR-001 Medium: 実機UARTの複数応答を固定長FIFOで到着順に保持し、対象TAG応答を失わないようにする。

完了結果:

- T005-NR-001 Mediumはresolved
- PlatformIO native T-005 test 12/12、T-004 test 13/13、T-003 test 5/5成功
- M5StickS3 clean/full build成功
- RAM 52,088 / 327,680 bytes（15.9%）
- Flash 1,217,423 / 3,342,336 bytes（36.4%）
- fix verification`pass_with_held`、新規findingなし
- 実機RYUW122応答、300ms超の遅延応答、5件以上のUART burstは後続へ保留

### T-006 複数TAG測距プロトコルとcodec

変更対象:

- `include/SequentialRangingProtocol.h`
- `src/SequentialRangingProtocol.cpp`
- 関連テスト

実施内容:

- 測距制御、逐次結果、フォロワー転送、ラウンド完了packetを定義する。
- wire enumを明示幅へ変換し、magic、version、長さ、session、indexを検証する。
- 最大8 ANCHOR、最大8 TAGの経路を表現する。
- 結果を1件ずつ送信し、ESP-NOW v1の250バイト上限内に保つ。

完了条件:

- 全packetのencode/decode正常系と基本的な長さ・version不正テストが成功する。
- 全wire構造体へ250バイト以下の`static_assert`がある。
- `enum class`の生サイズや構造体の暗黙paddingへ依存しない。
- M5StickS3 buildが成功する。
- T-006だけのコミットを作成する。

完了結果:

- 測距命令45 bytes、逐次測距結果117 bytes、ラウンド完了58 bytesの固定長packetを実装
- 全packetが250 bytes以下で、session、round、packet sequence、pair sequence、ノード順序を検証可能
- PlatformIO native T-006 test 9/9、既存回帰test 30/30成功
- M5StickS3 clean/full build成功
- RAM 52,088 / 327,680 bytes（15.9%）
- Flash 1,217,423 / 3,342,336 bytes（36.4%）
- 通常レビュー`pass_with_held`、必須修正findingなし
- 実機相互通信と異種endian相互運用は保留

### T-007 最短周期の順次測距状態機械

変更対象:

- `include/SequentialRangingController.h`
- `src/SequentialRangingController.cpp`
- 関連テスト

実施内容:

- ANCHOR ID外側、TAG ID内側の二重ループを実装する。
- 各測距完了時に1件を直ちにマスターTAGへ送信する。
- 同じANCHORの次TAG測距とESP-NOW結果送信を可能な限り並行させる。
- 次ANCHORへの制御を結果転送より優先する。
- マスターで1件ずつ公開し、フォロワーTAGへ対象結果を逐次転送する。
- 二重公開防止、基本timeout、欠損、旧session破棄を実装する。
- 最終結果受信直後に次ラウンドを開始する。

完了条件:

- `A1-T1, A1-T2, A2-T1, A2-T2, A3-T1, A3-T2`順序をテストで確認する。
- 全ラウンド完了前に各測距結果を取得できる。
- 50msなどの固定待機がない。
- UWB失敗やpacket欠損があってもラウンドtimeoutで無限待機しない。
- マスター交代で旧ラウンドが停止し、再同期後に再開する。
- ホストテストとM5StickS3 buildが成功する。
- T-007だけのコミットを作成する。

通常レビューfinding:

- T007-NR-001 Medium: 次ラウンド開始時にもNTP同期完了を再確認する。
- T007-NR-002 Medium: control sequence履歴をroundと正当な送信元の変更に対応させる。
- T007-NR-003 Low: フォロワーTAGで遅延した旧round完了通知を拒否する。
- T007-NR-004 Low: 3 ANCHOR×2 TAGの接続end-to-end testを追加する。

完了結果:

- T007-NR-001からT007-NR-004はすべてresolved
- PlatformIO native T-007 test 13/13、T-003からT-007の全回帰52/52成功
- 3 ANCHOR×2 TAGで`A1-T1, A1-T2, A2-T1, A2-T2, A3-T1, A3-T2`と次round即時開始を検証
- M5StickS3 clean/full build成功
- RAM 52,088 / 327,680 bytes（15.9%）
- Flash 1,218,015 / 3,342,336 bytes（36.4%）
- fix verification`pass_with_held`、新規findingなし
- 実機複数ノード、packet loss、queue飽和、master交代は後続へ保留

### T-008 アプリケーション統合と逐次表示

変更対象:

- `src/main.cpp`
- 必要な画面表示クラス
- `ConfigRuntime`およびNT-Shell連携の必要箇所

実施内容:

- 新規クラスをcompositionし、`main.cpp`を初期化、更新、結果取得、画面表示に限定する。
- ステータスへノードID、マスター・フォロワー状態、同期品質を表示する。
- 受信した逐次測距結果をSerialではなく画面へ表示する。
- 画面描画とNT-Shellで次測距開始を遅延させない。
- `wifi_power_save`を既存`pref`コマンドから確認・設定できることを確認する。

完了条件:

- ANCHOR、マスターTAG、フォロワーTAGの各モードで初期化経路が成立する。
- 測距結果1件ごとに画面更新要求が発生する。
- `main.cpp`へNTP計算、packet解析、二重ループ制御がない。
- M5StickS3 clean buildが成功する。
- T-008だけのコミットを作成する。

通常レビューfinding:

- T008-NR-001 Medium: 表示更新時にmeasurementとsummary FIFOを全件取り出し、最新状態を1回描画する。
- T008-NR-002 Medium: RYUW122、ESP-NOW transport、broadcastの初期化失敗を永続表示する。
- T008-NR-003 Low: マスターrole、session、validity変更時に旧測距表示を破棄する。

完了結果:

- T008-NR-001からT008-NR-003はすべてresolved
- `SequentialRangingDisplay`へ逐次結果、round summary、NodeStatus、初期化状態の描画を分離
- PlatformIO native test 58/58成功
- M5StickS3 clean/full build成功
- RAM 68,112 / 327,680 bytes（20.8%）
- Flash 1,232,859 / 3,342,336 bytes（36.9%）
- fix verification`pass_with_held`、新規findingなし
- 実機画面視認性、複数ノード通信、NT-Shell同時操作は後続へ保留

### T-009 統合テスト、M5StickS3 build、文書同期

変更対象:

- `test/`以下の統合テスト
- `docs/sequential-ranging-time-sync.md`
- `tasks-status.md`
- `phases-status.md`

実施内容:

- 複数TAG、複数ANCHORのend-to-end状態機械をfake transportとfake UWBで検証する。
- マスター交代、時刻同期、逐次公開、基本timeoutを結合して検証する。
- clean build、静的なDoxygen・命名・packet size検査を行う。
- 実装結果と設計書、進捗ファイルを同期する。

完了条件:

- ホスト統合テストが成功する。
- M5StickS3 clean/full buildが成功する。
- 追加・変更した全関数の日本語Doxygen検査が成功する。
- 設計と実装に既知の不一致がない。
- 実機でしか確認できない項目が明記される。
- T-009だけのコミットを作成する。

統合検証finding:

- T009-IF-001 Medium: ANCHORにもNTP同期確定通知を送り、自ノード時刻をマスター時刻へ変換できるようにする。

通常レビューfinding:

- T009-NR-001 Medium: 設計書のwire packet名、型、全field、サイズを実装と一致させる。
- T009-NR-002 Low: NTP DoxygenをフォロワーTAG限定から全非マスターノード向けへ修正する。

完了結果:

- T009-IF-001、T009-NR-001、T009-NR-002はすべてresolved
- production 6層直結の統合testで選出、全target NTP、3 ANCHOR×2 TAG、timeout、master交代、再同期を確認
- PlatformIO native test 60/60成功
- M5StickS3 clean/full build成功
- RAM 68,112 / 327,680 bytes（20.8%）
- Flash 1,232,811 / 3,342,336 bytes（36.9%）
- `docs/feature-list.md`を追加し、M5Stack移植前提の機能・設定・境界・保留項目を整理
- fix verification`pass_with_held`、新規findingなし
- 実機3 ANCHOR×2 TAG、packet loss、queue飽和、clock drift、M5Stack各機種は保留

### T-010 sol high最終レビューと必要修正

変更対象:

- レビューで必要と判断された範囲
- `tasks-status.md`
- `phases-status.md`

実施内容:

- 新規の`gpt-5.6-sol`、reasoning effort `high` sub-agentで最終レビューを行う。
- 指摘がある場合は同じタスク内で修正、再検証する。
- ユーザー指示により、レビュー担当ごとに異なるfindingを強制しない。

完了条件:

- 必須修正findingが残っていない。
- 修正後のホストテストとM5StickS3 clean/full buildが成功する。
- 最終状態を追跡ファイルへ反映する。
- T-010だけのコミットを作成する。

独立最終レビューfinding:

- T010-IFR-001 Medium: 未所有packetが共有ESP-NOW受信FIFOを恒久的に先頭blockしないterminal ownerを追加する。
- T010-IFR-002 Medium: consumer順と逆の既知packetをterminal ownerが破棄しない進捗handshakeを追加する。

完了結果:

- T010-IFR-001 Medium、T010-IFR-002 Mediumはいずれもresolved
- `EspNowReceiveQueueTerminator`とtransport consume counterで未知packetの最終所有を実装
- 逆順known、unknownからknown、multiple unknown、新着延期をproduction更新順で検証
- PlatformIO native test 69/69成功
- M5StickS3 clean/full build成功
- RAM 68,136 / 327,680 bytes（20.8%）
- Flash 1,233,375 / 3,342,336 bytes（36.9%）
- 独立最終fix verification`pass_with_held`、新規findingなし、unexploredなし
- 実機3 ANCHOR×2 TAG、packet loss、queue飽和、clock drift、M5Stack各機種は保留

### T-011 RYUW122 GPIO8リセット復旧

変更対象:

- `include/Ryuw122Initializer.h`
- `src/Ryuw122Initializer.cpp`
- `include/Ryuw122Controller.h`
- `src/Ryuw122Controller.cpp`
- `docs/sequential-ranging-time-sync.md`
- `docs/feature-list.md`
- 関連PlatformIO native test

実施内容:

- GPIO8をRYUW122のNRST制御へ使用する。
- UART開始、NRST復旧、AT疎通と設定を`Ryuw122Initializer`へ集約する。
- UART初期化後にNRSTをLOWで200ms保持し、入力へ戻してハイインピーダンス開放する。
- リセット開放後に1秒以上待ってからAT通信と既存設定を行う。
- modeを書き換えた場合だけ次のATコマンドまで2秒待機する。
- UART TXのLOWまたはフロートでRYUW122が無応答になる問題を起動時に復旧する。
- 通信確認失敗時に同じ復旧シーケンスを限定回数だけ再実行する。

完了条件:

- GPIO8のLOW、ハイインピーダンス開放、待機、AT初期化の順序をホストテストで確認できる。
- G7 TX、G1 RX、115200bpsの既存初期化と非同期測距が維持される。
- 全追加・変更関数へ日本語Doxygenがある。
- 全native testとM5StickS3 clean/full buildが成功する。
- 参照資料と異なる実機依存項目が明記される。
- T-011だけのコミットを作成する。

結果:

- focused native test 19/19成功
- PlatformIO native test 76/76成功
- M5StickS3 clean/full build成功
- RAM 68,144 / 327,680 bytes（20.8%）
- Flash 1,233,591 / 3,342,336 bytes（36.9%）
- 通常review finding `T011-NR-001` Mediumは修正済み
- fix verificationは`pass_with_held`、新規findingなし、unexploredなし
- 実装前調査、実装、通常review、修正、修正検証を`reports/T-011-*`へ保存済み
- 新しい再利用可能な開発手順上のSkill gapはなく、追加要件はRYUW122固有の製品設計として記録した。

### T-012 計測開始シーケンスと実装範囲の明文化

変更対象:

- `docs/sequential-ranging-time-sync.md`
- `docs/feature-list.md`
- `tasks-status.md`
- `phases-status.md`

実施内容:

- 起動からNodeStatus収集、最小TAG IDのマスター更新、NTP四時刻同期、初回`RangeControl`、最初のUWB測距開始までを1つのシーケンス図にする。
- マスター識別情報またはsession更新時に旧同期と旧roundを破棄し、新マスターともう一度同期してから測距を再開する流れを明記する。
- 現在の実装範囲が距離の逐次集約・公開・対象TAG転送までで、座標計算とEKFは未実装であることを明記する。

完了条件:

- 初回500msのマスター選出待ちと最小TAG IDの選出が図に含まれる。
- 全非マスターノードの再同期完了前に測距を開始しないことが図と本文で確認できる。
- 最小IDのANCHORへの初回`RangeControl`と最初のUWB測距開始が図に含まれる。
- 既存実装と文書の役割・結果集約先が一致する。
- Markdown構造とMermaid code fenceが整合する。

結果:

- 起動から初回UWB測距開始までのMermaidシーケンス図を追加した。
- 初回マスター選出と、マスター更新後に全有効非マスターノードが新マスターともう一度同期する流れを明記した。
- 距離結果の集約・逐次公開・対象TAG転送までが実装済みで、座標計算、EKF、永続履歴は未実装であることを明記した。
- 製品コードとテストコードは変更していない。
- Markdown lintはrepository配線がないためunsupported、構造・code fence・末尾空白・差分検査は成功した。

### T-013 接続先3件とTAG全ANCHOR測距結果・統一時刻の画面表示

変更対象:

- `include/SequentialRangingDisplay.h`
- `src/SequentialRangingDisplay.cpp`
- `include/NtpTimeSynchronizer.h`
- `src/NtpTimeSynchronizer.cpp`
- `src/main.cpp`のcomposition
- `test/test_t008/`以下の関連テスト
- `docs/sequential-ranging-time-sync.md`
- `docs/feature-list.md`

実施内容:

- 受信済みNodeStatusの画面表示上限を2件から3件へ拡張する。
- TAGでは、自ノードへ公開された測距結果をANCHOR IDごとに最大8件保持し、全ANCHORの最新距離を画面へ表示する。
- 各ANCHORの距離とともに、マスターTAG基準の計測完了時刻を表示する。
- TAG画面にマスターTAG基準の現在時刻を表示する。
- 表示判断、一覧保持、描画を`SequentialRangingDisplay`へ集約し、`main.cpp`は依存注入と表示クラス呼び出しだけに保つ。

完了条件:

- 接続先3件が同時に画面へ描画されることをホストテストで確認できる。
- マスターTAGでは他TAG向け結果を除外し、フォロワーTAGでは自TAG向け転送結果を使用して、全ANCHORの最新距離をホストテストで確認できる。
- 各ANCHOR行のマスター基準計測時刻と、TAG画面のマスター基準現在時刻を確認できる。
- ANCHOR表示と既存初期化失敗表示に回帰がない。
- 全追加・変更関数へ日本語Doxygenがある。
- PlatformIO native testとM5StickS3 clean/full buildが成功する。
- T-013だけのコミットを作成する。

通常レビューfinding:

- T013-NR-001 Medium: 未同期measurementを有効なマスター時刻のように表示せず、現在時刻と計測時刻を共通の表示基準へそろえる。

結果:

- TAGは自TAG向けmeasurementだけをANCHOR ID別に最大8件保持し、ANCHOR ID昇順で距離または失敗状態とマスター基準計測時刻を表示する。
- マスターTAGが収集した他TAG向け結果を除外し、フォロワーTAGは自TAG向け転送結果を表示する。
- 現在のマスター基準時刻を`NOW`行へ表示し、未同期時は`UNSYNC`とする。
- 受信NodeStatusの先頭3件と最大8 ANCHOR結果が135×240画面内に収まる。
- `main.cpp`の変更は既存`NtpTimeSynchronizer`を表示クラスへ注入するcomposition 1行だけである。
- T013-NR-001 Mediumは修正済みで、fix verificationは`pass_with_held`、新規findingとunexploredはない。
- focused native_t004 16/16、native_t008 12/12、全native 84/84、M5StickS3 clean/full build、`git diff --check`が成功した。
- M5StickS3 build使用量はRAM 68,624 / 327,680 bytes、Flash 1,234,447 / 3,342,336 bytesである。
- 実機での最大8 ANCHOR通信、文字視認性、ちらつき、長時間折り返しは保留する。

## 実機保留項目

次はコードとbuildだけでは完了判定できないため、対応する実機が揃うまで保留として扱う。

- TAG 2台、ANCHOR 3台以上での実測順序
- RYUW122の実測最短周期と300ms timeoutの妥当性
- ESP-NOW packet loss時の更新間隔
- NTP offset、RTT、ESP32個体間clock drift
- マスターTAG電源断および低ID TAG途中参加時の交代
- Wi-Fi Power Save ON/OFFによる受信timestamp品質差
- GPIO8とRYUW122 NRSTの実配線、電圧、立上り、UARTウェッジからの実復旧
- mode変更後2秒待機による実機AT通信成功と起動体感
