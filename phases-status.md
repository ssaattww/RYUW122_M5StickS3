# 複数TAG順次測距 フェーズ状況

このファイルは`task-breakdown-planner`、`task-consistency-manager`、`progress-sync-manager`を通じてのみ更新する。

## 全体状況

- 総フェーズ数: 10
- 完了フェーズ数: 10
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
| P8 | 計測開始フロー文書化 | T-012 | 完了 | マスター更新を含む起動から初回測距開始までのシーケンスと実装範囲が明確である |
| P9 | 画面表示改善 | T-013 | 完了 | 接続先3件、TAGごとの全ANCHOR距離、計測時刻、現在統一時刻をmain外の表示クラスで確認できる |
| P10 | NTP実機復旧 | T-014 | 完了 | 同一時計基準、後参加、失敗再試行、30秒周期再同期を実機向け経路で確認できる |

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

### P8 計測開始フロー文書化

成果物:

- 起動から初回UWB測距開始までのシーケンス図
- マスター更新後に新マスターと再同期するシーケンス
- 距離集約までの実装済み範囲と座標計算・EKFの未実装境界

終了判定:

- マスター選出、マスター更新、NTP再同期、初回`RangeControl`、最初のUWB測距開始が一続きで確認できる。
- マスター更新時に全非マスターノードがもう一度同期することが明記されている。
- 設計書と機能一覧が現在の実装に一致する。

### P9 画面表示改善

成果物:

- 接続先3件のNodeStatus一覧表示
- TAGごとの全ANCHOR最新距離、計測時刻、現在統一時刻表示
- `SequentialRangingDisplay`のホストテストとM5StickS3 build結果

終了判定:

- 受信済み接続先を最大3件まで画面へ表示する。
- マスターTAGとフォロワーTAGの双方で、自ノードに対する全ANCHORの最新距離とマスター基準計測時刻を画面へ表示する。
- TAG画面にマスターTAG基準の現在時刻を表示する。
- `main.cpp`は依存のcompositionと表示クラス呼び出しだけに保ち、時刻変換、一覧保持、描画を表示・同期クラスへ集約する。
- PlatformIO native testとM5StickS3 clean/full buildが成功する。

完了結果:

- TAG画面は自TAG向け結果をANCHOR ID別に最大8件保持し、ID順に距離または失敗状態とマスター基準計測時刻を表示する。
- TAG画面はマスター基準の現在時刻を表示し、未同期時は`UNSYNC`とする。
- NodeStatus先頭3件と最大8 ANCHOR結果が135×240画面内に収まる。
- 通常レビューfinding `T013-NR-001` Mediumは修正済みで、fix verificationは`pass_with_held`である。
- focused testはnative_t004 16/16、native_t008 12/12、全native 84/84、M5StickS3 clean/full buildが成功した。

### P10 NTP実機復旧

成果物:

- ESP Timerへ統一したNTP四時刻取得
- 後参加ノードの即時同期
- 初回失敗時の1秒再試行と30秒周期再同期
- host testとM5StickS3 build結果

終了判定:

- `rx_ctrl->timestamp`をNTP四時刻計算へ使用しない。
- 電源投入時刻が異なるノードを再起動なしで同期対象へ追加できる。
- 全同期失敗で`ReadyToStart`へ誤遷移せず、再試行できる。
- 正常同期後30秒で現在有効なノードと同期を更新できる。
- 通常buildとテスト専用native環境の入口が分離されている。
- PlatformIO native testとM5StickS3 clean/full buildが成功する。

完了結果:

- NTP四時刻はESP Timer時計基準へ統一された。
- 後参加ノードの即時同期、失敗後1秒再試行、正常完了後30秒周期再同期を実装した。
- 周期再同期では消失ノードを除外し、次round開始前に同期完了を要求する。
- 引数なし通常buildをM5StickS3へ限定し、native環境をTest Runner専用として分離した。
- focused native 21/21、全native 89/89、M5StickS3 clean/full buildが成功した。
- ユーザー指示によりレビューは省略した。
