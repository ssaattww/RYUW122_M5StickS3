# Sub-agent実行レポート

## タスク

- 目的: T015-NR-001からT015-NR-004の修正を同一reviewerが検証する。
- タスク種別: 修正検証

## sub-agentを使う理由

- 理由: findingのidentityとseverityを維持し、通常レビュー担当が修正と直接影響範囲を確認するため。

## 対象範囲

- 対象: T-015 initial normal reviewの同一reviewerによるfix verification。review targetはbase/HEAD `6c82a4bc63bce38f905d3de17eda60b581b31b3b`と、レビュー開始時点のT-015未コミットworking-tree差分の合成である。開始時に予約済みの本レポートを除く差分対象32ファイルのSHA-256 manifestを固定し、終了時に再計算して同一性を確認した。T015-NR-001からT015-NR-004までの直接修正、同系統箇所、新規回帰、製品コード、host test、設計文書、tracking、実装レポート、buildを確認した。coverage dispositionは、要件/設計=`checked_finding`、正しさ/edge case=`checked_no_finding`、scope discipline=`checked_no_finding`、変更ファイル/直接依存=`checked_finding`、API/data/config/workflow互換性=`checked_finding`、error handling=`checked_no_finding`、security/secrets=`not_applicable`、tests/validation=`checked_no_finding`、CI=`not_applicable`（未コミット合成targetに一致するrunなし、結論unknown）、report/tracking/docs=`checked_finding`、回帰/保守性=`checked_finding`、coding standards=`checked_no_finding`とした。

## 対象外

- 対象外: fix実装、製品コード/test/docs/tracking/他reportの編集、Git stage/commit/push/PR/merge、実機でのみ観測できるtask scheduling・表示・無線/UWB挙動の成功認定、独立最終レビューattestation。`report_attestation_allowed=false`。

## 実行コマンド

- 実行コマンド: `git status --short --branch`、`git rev-parse HEAD`、`git diff --name-only`、`git ls-files --others --exclude-standard`、`Get-FileHash -Algorithm SHA256`、`git diff --check`、`platformio test -e native_t008`、`platformio test -e native_t015`、`platformio test -e native_t001 -e native_t002 -e native_t003 -e native_t004 -e native_t005 -e native_t006 -e native_t008 -e native_t014 -e native_t015`、`platformio run -e m5stack-sticks3 -t clean`、`platformio run -e m5stack-sticks3`、`rg`によるmode request/BtnA/SetRunMode、task ownership、Doxygen・命名、設計/追跡整合の横断確認。最初の非昇格`platformio test -e native_t008`はPlatformIO cache lockへのPermissionErrorでexit 1となり、環境制約として失敗を保持したうえで、同一コマンドを許可済み環境で再実行した。Markdown wording checkはrepoに`tools/lint/`、`package.json`、`cspell.config.jsonc`がなく、focused/fullとも`unsupported`でありpass扱いしていない。

## 対象ファイル

- 変更または確認したファイル: `include/RangingDisplayTaskController.h`、`src/RangingDisplayTaskController.cpp`、`src/main.cpp`、`include/SequentialRangingDisplay.h`、`src/SequentialRangingDisplay.cpp`、`platformio.ini`、`test/test_t015/test_main.cpp`と全stub、`test/test_t008/test_main.cpp`とstub、`test/README.md`、`docs/sequential-ranging-time-sync.md`、`docs/feature-list.md`、直接依存文書`docs/current-class-architecture.md`、`tasks-status.md`、`phases-status.md`、`reports/T-015-ranging-display-task-normal-review.md`、`reports/T-015-ranging-display-task-implementation.md`、`reports/T-015-ranging-display-task-fix-implementation.md`、予約済み`reports/T-015-ranging-display-task-fix-verification.md`。書き込みは本レポートのplaceholder置換だけである。

## 指摘事項

- 指摘要約または「指摘なし」: findings first（severity順）。
  - **T015-NR-002 / Medium / unresolved** — 製品コードからruntime mode request経路を削除し、`docs/sequential-ranging-time-sync.md`と`docs/feature-list.md`をNVS更新後の再起動契約へ合わせた直接修正は確認した。しかし、現行実装を説明する直接依存文書`docs/current-class-architecture.md:114`は依然として「`main.cpp`のBtnA処理が`ConfigRuntime::SetRunMode()`を呼ぶ」と記載する。さらに同文書`docs/current-class-architecture.md:122-133`は通信・測距・描画を現在の`loop()`順序として示し、`docs/current-class-architecture.md:224-257`は`RangingDisplayTaskController`を生成/開始順から欠落させている。実装済みの「runtime切替なし、NVS変更後再起動」とtask ownershipを、保守者が現在の契約として誤認するため、元findingの直接影響がdocs側に残る。sibling確認ではT-015の2設計文書と製品コードにmode request経路がないことを確認した。`docs/current-class-architecture.md`の現在構成、依存図、責務、実行順、生成/開始順を実装へ同期する必要がある。
  - **T015-NR-001 / Medium / resolved** — `RangingDisplayTaskController::Begin()`はsnapshot queue、測距task、表示taskの各失敗でfalseを返し、`End()`が作成済み表示task、測距task、queueの順に解放してhandleをnull化する。`src/main.cpp:105-108`はfalseを検出し、`ShowTaskStartFailure()`で永続診断を表示する。queue失敗、測距task失敗、表示task部分失敗、明示`End()`の削除順をproduction controller経由のtestで確認した。直接影響と同系統失敗に未解消なし。
  - **T015-NR-003 / Medium / resolved** — `[env:native_t015]`は`src/RangingDisplayTaskController.cpp`を直接compile/linkし、6 testでproduction `Begin()`、task entry、priority/core、更新順、capacity 1 snapshot overwrite、最新snapshot描画、3種類の開始失敗cleanup/診断、`End()`順序を実行する。stubはFreeRTOS/M5/依存objectの呼出しを記録する薄い境界で、製品分岐の代替実装にはなっていない。静的確認でも高優先taskにM5/Canvas/Serial処理がなく、低優先taskがprotocol所有objectへ直接触れないことを確認した。
  - **T015-NR-004 / Low / resolved** — `phases-status.md:195`の「レビュー省略」はP10/T-014の完了記録内へ移動し、P11は`phases-status.md:197`から独立している。T-014/P10とT-015/P11の意味上の混同は解消した。
  - **New findings: なし。Severity reclassification/errata: なし。** 日本語Doxygen、`En` enum、UpperCamelCase、`m_lowerCamelCase`、class/file一致にも追加指摘なし。

## 結果

- 結果: **verdict=`fail`**。T015-NR-001、T015-NR-003、T015-NR-004はresolved、T015-NR-002はMediumのままunresolvedである。validationは、focused `native_t008`=13/13 pass、focused `native_t015`=6/6 pass、全native=96/96 pass、M5 clean=pass、clean後full build=pass（RAM 68,808/327,680 bytes、21.0%、Flash 1,236,167/3,342,336 bytes、37.0%）、`git diff --check`=exit 0（LFからCRLFへのwarningのみ）だった。開始/終了fingerprintは一致し、review targetはstable。CIはmatching runなしのため`not_applicable/unknown`であり、成功とは扱わない。

## リスク

- 未解決のリスクまたは後続対応: 次actionはT015-NR-002として`docs/current-class-architecture.md`から削除済みBtnA/`SetRunMode()`契約を除き、`RangingDisplayTaskController`、高/低優先task、snapshot queue、現在の`loop()`、生成/開始順を実装へ同期した後、同一reviewerで再fix verificationすること。heldは実機上のtask/queue確保失敗表示の持続性、core間並行性と`pushSprite()`非block性、ANCHOR RANGEからIDLEへの表示遅延、M5Unified/Canvasのtask affinity、ESP32 system/Wi-Fi taskへの影響、task stack high-water、starvation/watchdog、ESP-NOW/RYUW122実通信、packet loss/queue saturation/clock drift/NT-Shell併用である。これらはcode findingと分離し、native/build成功で解消扱いにしない。unexploredはなし。Markdown lint wiring欠如は`unsupported`の残余リスクである。
