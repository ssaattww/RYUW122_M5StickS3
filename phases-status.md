# 複数TAG順次測距 フェーズ状況

このファイルは`task-breakdown-planner`、`task-consistency-manager`、`progress-sync-manager`を通じてのみ更新する。

## 全体状況

- 総フェーズ数: 7
- 完了フェーズ数: 7
- 現在フェーズ: なし
- 次フェーズ: なし
- 実装開始条件: ユーザーによる設計・タスク分解の確認
- 実行方式: 1タスクずつ実装、検証、進捗同期、個別コミット
- 実装優先度: 正常系を先に成立させ、複雑な再送・障害復旧は初期対象外とする
- 証跡: 各タスクの実装・検証・レビュー結果を`reports/`へ保存する

## フェーズ一覧

| ID | フェーズ | 対象タスク | 状態 | 完了条件 |
| --- | --- | --- | --- | --- |
| P1 | 設計・実行準備 | T-001 | 完了 | 設計書、タスク分解、依存関係、コミット規則が確定している |
| P2 | ESP-NOW通信とマスター選出 | T-002, T-003 | 完了 | raw ESP-NOW基盤と最小TAG IDの選出がbuild・テスト済みである |
| P3 | 時刻同期とUWB測距基盤 | T-004, T-005 | 完了 | NTP時刻変換と非同期RYUW122測距が独立して検証済みである |
| P4 | 複数TAG順次測距 | T-006, T-007 | 完了 | packetと二重ループ状態機械が逐次結果を最短周期で公開する |
| P5 | アプリケーション統合 | T-008, T-009 | 完了 | 画面統合、ホストテスト、M5StickS3 clean/full buildが成功する |
| P6 | 最終確認 | T-010 | 完了 | sol highレビューの必須修正がなく、最終検証が成功する |
| P7 | RYUW122 UART復旧 | T-011 | 完了 | GPIO8のNRST制御で起動時UARTウェッジを復旧できる |

## フェーズ詳細

### P1 設計・実行準備

成果物:

- 複数TAG順次測距とマスターTAG時刻同期の設計書
- 正常系、NTP同期、マスター交代のシーケンス図
- タスク・フェーズ追跡

終了判定:

- 全ノードIDは一意という前提が記録されている。
- 最小TAG IDをマスターとする規則が記録されている。
- `A1-T1, A1-T2, A2-T1...`の測距順と1件ごとの逐次公開が記録されている。

### P2 ESP-NOW通信とマスター選出

成果物:

- raw `esp_now_recv_info_t`対応transport
- NVS切替可能なWi-Fi Power Save
- 拡張NodeStatusと`m_nodes`
- `TagMasterCoordinator`

終了判定:

- ESP-NOW callbackから必要情報だけ安全にコピーできる。
- `wifi_power_save=false`が既定値として適用される。
- 複数TAGから最小IDのマスターを決定できる。

### P3 時刻同期とUWB測距基盤

成果物:

- `NtpTimeSynchronizer`
- `Ryuw122Controller`の非同期測距API

終了判定:

- 全非マスターノードをマスターTAG時刻へ対応付けられる。
- G1/G7、115200bpsのRYUW122でブロッキングせず測距状態を更新できる。

### P4 複数TAG順次測距

成果物:

- 250バイト以内の逐次測距protocol
- `SequentialRangingController`

終了判定:

- ANCHOR外側、TAG内側のID昇順で動作する。
- 各結果を全ラウンド完了前に公開できる。
- 最終結果受信直後に次ラウンドを開始できる。

### P5 アプリケーション統合

成果物:

- 最小責務の`main.cpp`
- 逐次測距結果とID・マスター状態の画面表示
- ホスト統合テストとM5StickS3 build結果

終了判定:

- main、画面、NT-Shellが測距状態機械を妨げない。
- host testとM5StickS3 clean/full buildが成功する。
- 実装と設計・追跡が一致する。

### P6 最終確認

成果物:

- sol highレビュー結果
- 必要な修正と再検証

終了判定:

- 必須修正findingが残っていない。
- 最終コミットがタスク単位で分離されている。
- 実機保留項目とコード上の完了項目が区別されている。

### P7 RYUW122 UART復旧

成果物:

- GPIO8を使用するRYUW122 NRST制御
- 起動時と通信失敗時の限定復旧シーケンス
- host testとM5StickS3 build結果

終了判定:

- UART初期化後にNRST LOW 200ms、ハイインピーダンス開放、1秒以上待機の順序が成立する。
- mode変更後だけ次のATコマンドまで2秒待機する。
- G7/G1、115200bps、既存測距処理に回帰がない。
- 設計、機能一覧、実装が一致する。
- focused native test 19/19、全native test 76/76、M5StickS3 clean/full buildが成功している。
- 通常review findingが解消され、fix verificationが`pass_with_held`である。
