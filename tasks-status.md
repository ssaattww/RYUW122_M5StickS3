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
- 実装対象は`C:\Users\taiga\Documents\PlatformIO\Projects\RYUW122_M5StickS3`とし、`.pio/libdeps`は編集しない。

## 現在位置

- 現在フェーズ: P2 ESP-NOW通信とマスター選出
- 完了タスク: T-001, T-002
- 次タスク: T-003 NodeStatus拡張とマスターTAG選出
- 次タスク状態: 未着手
- ブランチ: `codex/multitag-sequential-ranging`

## タスク一覧

| ID | フェーズ | タスク | 見積 | 依存 | 状態 | コミット |
| --- | --- | --- | --- | --- | --- | --- |
| T-001 | P1 | 設計確定とタスク分解 | M | なし | 完了 | `75bb8af` + 本追跡コミット |
| T-002 | P2 | ESP-NOW transportとWi-Fi設定基盤 | L | T-001 | 完了 | 本タスクのコミット |
| T-003 | P2 | NodeStatus拡張とマスターTAG選出 | M | T-002 | 未着手 | 未作成 |
| T-004 | P3 | NTP四時刻同期とマスター時刻変換 | L | T-003 | 未着手 | 未作成 |
| T-005 | P3 | RYUW122非同期測距API | L | T-001 | 未着手 | 未作成 |
| T-006 | P4 | 複数TAG測距プロトコルとcodec | M | T-003, T-004 | 未着手 | 未作成 |
| T-007 | P4 | 最短周期の順次測距状態機械 | XL | T-004, T-005, T-006 | 未着手 | 未作成 |
| T-008 | P5 | アプリケーション統合と逐次表示 | L | T-007 | 未着手 | 未作成 |
| T-009 | P5 | 統合テスト、M5StickS3 build、文書同期 | L | T-008 | 未着手 | 未作成 |
| T-010 | P6 | sol high最終レビューと必要修正 | M | T-009 | 未着手 | 未作成 |

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

## 実機保留項目

次はコードとbuildだけでは完了判定できないため、対応する実機が揃うまで保留として扱う。

- TAG 2台、ANCHOR 3台以上での実測順序
- RYUW122の実測最短周期と300ms timeoutの妥当性
- ESP-NOW packet loss時の更新間隔
- NTP offset、RTT、ESP32個体間clock drift
- マスターTAG電源断および低ID TAG途中参加時の交代
- Wi-Fi Power Save ON/OFFによる受信timestamp品質差
