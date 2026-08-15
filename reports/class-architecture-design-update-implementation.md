# クラス責務・クラス間関係 設計追記レポート

## 1. 対象

- repository: `ssaattww/RYUW122_M5StickS3`
- base branch: `master`
- base HEAD: `ac05beb959ef01eda231da4083ed87f240fb37bc`
- work branch: `agent/document-class-relationships`
- design change commit: `b6e2228a7bcb2810026d240ff2107476ca81b375`
- task: 現在のクラスの役割とクラス間の関係を設計書へ記録し、PRを作成する

## 2. 調査した実装

現在の`master`を基準に、次の実装境界を確認した。

- `src/main.cpp`: composition、`setup()`開始順、`loop()`更新順
- `include/ConfigPreference.h`, `include/ConfigRuntime.h`: 永続設定と実行時設定の分離
- `include/NvsPreferenceStore.h`, `include/PreferenceCommands.h`, `include/NtShell.h`, `src/NtShell.cpp`: NVSとNT-Shellの境界
- `include/NodeStatus.h`, `include/EspNowTransport.h`, `include/EspNowBroadcast.h`: NodeStatus codecとESP-NOW共有transport
- `include/EspNowReceiveQueueTerminator.h`: 共有受信FIFOの最終所有境界
- `include/TagMasterCoordinator.h`, `src/TagMasterCoordinator.cpp`: TAG master選出とNodeStatusへのmaster state反映
- `include/NtpTimeProtocolCodec.h`, `include/NtpTimeSynchronizer.h`: NTP wire codecと時刻同期
- `include/Ryuw122Controller.h`, `src/Ryuw122Controller.cpp`: `IRyuw122Port`抽象化、実機port、非同期測距
- `include/SequentialRangingProtocolCodec.h`, `include/SequentialRangingController.h`: 逐次測距wire codecと状態機械
- `include/SequentialRangingDisplay.h`: 表示境界
- `docs/sequential-ranging-time-sync.md`: 既存の複数TAG順次測距・時刻同期設計

## 3. 変更内容

`docs/current-class-architecture.md`を追加し、現在実装されているクラス構成を実装基準で整理した。

記録した内容は次のとおり。

1. `main.cpp`をcomposition rootとした全体依存関係
2. 公開クラス、codec、port interface、実装内部クラスを含むクラス別責務表
3. `NvsPreferenceStore` → `ConfigPreference` → `ConfigRuntime`の設定経路
4. `PreferenceCommands` → `NtShell`のコマンド登録境界
5. `EspNowTransport`を共有する受信FIFO consumerと`EspNowReceiveQueueTerminator`の所有境界
6. `EspNowBroadcast` → `TagMasterCoordinator` → `NtpTimeSynchronizer` / `SequentialRangingController`の依存関係
7. `IRyuw122Port`、`Ryuw122HardwarePort`、`Ryuw122Controller`の抽象化関係
8. `SequentialRangingController`から表示までの入出力関係
9. 現在のグローバルオブジェクト生成順、`setup()`開始順、`loop()`更新順
10. Mermaid class diagramとESP-NOW更新順flowchart

## 4. 設計上明文化した重要境界

### 4.1 ESP-NOW受信

ESP-NOW callbackを所有するのは`EspNowTransport`だけであり、受信packetは共有FIFOへ固定長コピーされる。

既知consumerは次の3クラスで、FIFO先頭が自身のpacket種別の場合だけ消費する。

- `EspNowBroadcast`
- `NtpTimeSynchronizer`
- `SequentialRangingController`

`EspNowReceiveQueueTerminator`はこれらの後段で、既知consumerがcycle中にFIFOを進めなかった場合だけ、cycle開始時から存在した未所有packetを最大1件破棄する。

### 4.2 設定

NVSの永続値は`NvsPreferenceStore`と`ConfigPreference`、実行中の値は`ConfigRuntime`が担当する。
`PreferenceCommands`はNVSを書き換えるが`ConfigRuntime`を自動更新しないため、`pref`設定は次回起動時の`ConfigRuntime::Init()`で反映される。

### 4.3 RYUW122

`SequentialRangingController`はUARTやAT応答を直接扱わず、`Ryuw122Controller`の非同期APIを利用する。
実機UART処理は`IRyuw122Port`を実装する`Ryuw122HardwarePort`へ閉じ込められている。

## 5. 変更範囲

設計変更commit `b6e2228a7bcb2810026d240ff2107476ca81b375`をbase HEADと比較した結果は次のとおり。

- status: `ahead`
- ahead by: 1 commit
- behind by: 0 commits
- changed files: 1
- `docs/current-class-architecture.md`: added, 269 additions, 0 deletions

ソースコード、ビルド設定、テスト、workflow、既存設計ファイルには変更を加えていない。

## 6. 検証

実施した検証:

- GitHub connectorで現在のheaders、主要cpp、`main.cpp`を読み、設計記述を実装と突合した。
- 作成後の`docs/current-class-architecture.md`をGitHub connectorで再取得し、保存された内容を確認した。
- `master`のbase HEADと設計change commitをcompareし、設計ファイル1件だけの変更であることを確認した。
- `TagMasterCoordinator`が実際に`EspNowBroadcast::SetMasterState()`を呼ぶことを実装で再確認した。

未実施:

- firmware build
- host test
- 実機試験

理由: 今回は実行コードを変更しない文書追加のみであり、動作バイナリに変更がないため。

## 7. CI確認方針

PR作成後、PRのcurrent HEAD SHAを取得し、そのSHAとworkflow runの`head_sha`が一致するrunだけをCI証拠として扱う。

一致するrunが存在しない場合はCI未実施として報告し、別SHAのrunを代用しない。

## 8. 残作業

- 本レポートを保存したcommitを含むbranch HEADからPRを作成する。
- PR current HEAD SHAに一致するCI runを確認する。
- 変更内容と検証結果の簡易reportをPRコメントへ投稿する。
- mergeは実施しない。
