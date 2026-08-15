# Sub-agent実行レポート

## タスク

- 目的: T015-NR-002の追加文書修正を同一reviewerが再検証する。
- タスク種別: 修正検証

## sub-agentを使う理由

- 理由: findingのidentityとseverityを維持し、通常レビュー担当が残存不一致の解消を確認するため。

## 対象範囲

- 対象: T-015 initial normal reviewと第1回fix verificationを担当した同一reviewerによるT015-NR-002 Mediumの再fix verification。review targetはbase/HEAD `6c82a4bc63bce38f905d3de17eda60b581b31b3b`と、開始時点のT-015未コミットworking-tree差分の合成である。予約済みの本レポートを除く35ファイルのSHA-256 manifestを開始時に固定した。第1回fix verificationの32ファイルmanifestは今回も全hash一致し、新たな実質変更が`docs/current-class-architecture.md`だけであることを確認した。T015-NR-002の直接修正、関連実装・設計、同系統の旧契約、T015-NR-001/003/004の回帰、新規findingを確認した。coverage dispositionは、要件/設計=`checked_no_finding`、正しさ/edge case=`checked_no_finding`、scope discipline=`checked_no_finding`、変更ファイル/直接依存=`checked_no_finding`、API/data/config/workflow互換性=`checked_no_finding`、error handling=`checked_no_finding`、security/secrets=`not_applicable`、tests/validation=`checked_no_finding`、CI=`not_applicable`（未コミット合成targetに一致するrunなし、結論unknown）、report/tracking/docs=`checked_no_finding`、回帰/保守性=`checked_no_finding`とした。

## 対象外

- 対象外: fix実装、製品コード/test/docs/tracking/他reportの編集、Git stage/commit/push/PR/merge、文書限定差分に対するnative testとM5 buildの不要な再実行、実機専用事項の成功認定、独立最終レビューattestation。`report_attestation_allowed=false`。

## 実行コマンド

- 実行コマンド: `git rev-parse HEAD`、`git status --short --branch`、`git diff --stat`、`git diff -- docs/current-class-architecture.md`、`git diff --check -- docs/current-class-architecture.md`、最終`git diff --check`、`git diff --name-only`、`git ls-files --others --exclude-standard`、`Get-FileHash -Algorithm SHA256`、`Get-Content -Raw`、`rg`によるBtnA/`SetRunMode`/runtime mode/旧loop契約、task controller、priority/core、更新順、snapshot、NVS、失敗表示、回帰箇所の横断確認。Markdown fallbackとして見出し順、fence対、必須/禁止用語、相対link存在、末尾空白をPowerShellで検査した。repoに`tools/lint/`、`package.json`、`cspell.config.jsonc`がないためrepository-local focused/full Markdown lintは`unsupported`であり、fallback成功をlint成功へ読み替えていない。製品・test・build設定の第1回fix verification時hashが全て不変な文書限定修正のため、native testとM5 buildは再実行していない。

## 対象ファイル

- 変更または確認したファイル: 修正文書`docs/current-class-architecture.md`、関連設計`docs/feature-list.md`と`docs/sequential-ranging-time-sync.md`、実装`include/RangingDisplayTaskController.h`、`src/RangingDisplayTaskController.cpp`、`src/main.cpp`、test wiring `platformio.ini`と`test/test_t015/test_main.cpp`、tracking `phases-status.md`、`reports/T-015-ranging-display-task-fix-verification.md`、`reports/T-015-ranging-display-task-fix-implementation-2.md`、予約済み`reports/T-015-ranging-display-task-fix-verification-2.md`。書き込みは本レポートのplaceholder置換だけである。

## 指摘事項

- 指摘要約または「指摘なし」: findings first（severity順）。
  - **T015-NR-002 / Medium / resolved** — `docs/current-class-architecture.md`から旧BtnA/`ConfigRuntime::SetRunMode()`と旧`loop()`更新契約は除去された。`docs/current-class-architecture.md:40,77-85,111-113`は`RangingDisplayTaskController`の依存、責務、core 1・priority 4とcore 0・priority 1を示す。`docs/current-class-architecture.md:138-163`の高優先度更新順、capacity 1 snapshot上書き、低優先度描画順、待機のみのArduino `loop()`は`src/RangingDisplayTaskController.cpp:183-228`と`src/main.cpp:114-117`に一致する。`docs/current-class-architecture.md:125-130`はmodeを`pref set run_mode`でNVSへ設定し、再起動後の`ConfigRuntime::Init()`、RYUW122、NodeStatus、protocol状態へ一貫反映する契約であり、関連2設計文書とも一致する。`docs/current-class-architecture.md:253-296`の生成/開始順、`Begin()`のcapacity 1 queue・2 task作成、部分失敗cleanup、`TASK START FAILED`表示も実装と一致する。旧契約のsibling検索で肯定的なruntime切替経路は見つからなかった。
  - **T015-NR-001 / Medium / resolved（回帰なし）** — 前回targetのcontrollerと`main.cpp` hashは不変であり、`Begin()`のqueue/各task失敗時cleanup、false戻り値、`main.cpp:105-108`の失敗分岐と永続診断を再確認した。
  - **T015-NR-003 / Medium / resolved（回帰なし）** — `platformio.ini:162-174`と`test/test_t015/test_main.cpp`を含むtest/config hashは不変で、production `RangingDisplayTaskController.cpp`を直接compile/link/executeする6 testの契約は維持されている。
  - **T015-NR-004 / Low / resolved（回帰なし）** — `phases-status.md` hashは不変で、レビュー省略文はP10/T-014内の`phases-status.md:195`、P11は`phases-status.md:197`以降として分離されたままである。
  - **New findings: なし。Severity reclassification/errata: なし。**

## 結果

- 結果: **verdict=`pass_with_held`**。T015-NR-001、T015-NR-002、T015-NR-003、T015-NR-004は全てresolvedで、新規findingはない。第1回fix verificationのfocused `native_t008` 13/13、focused `native_t015` 6/6、全native 96/96、M5 clean/full build成功（RAM 68,808/327,680 bytes、21.0%、Flash 1,236,167/3,342,336 bytes、37.0%）は、対応する製品・test・build設定32ファイルのSHA-256完全一致により今回targetへ継続可能な既存証拠である。今回は文書限定diffのため再実行しておらず、新規実行成功とは主張しない。対象文書のMarkdown fallbackは見出し12件の順序、fence 16件の均衡、必須語、禁止された旧契約の不在、相対link、末尾空白を全てpassし、`git diff --check`もexit 0（改行warningのみ）だった。終了時manifestは開始時と一致し、targetはstable。CIはmatching runなしのため`not_applicable/unknown`である。

## リスク

- 未解決のリスクまたは後続対応: heldは実機上のtask/queue確保失敗表示の持続性、core間並行性と`pushSprite()`非block性、ANCHOR RANGEからIDLEへの表示遅延、M5Unified/Canvasのtask affinity、ESP32 system/Wi-Fi taskへの影響、task stack high-water、starvation/watchdog、ESP-NOW/RYUW122実通信、packet loss/queue saturation/clock drift/NT-Shell併用であり、文書修正では解消しない。unexploredはなし。repository-local Markdown lint wiring欠如は`unsupported`の残余リスクだが、今回の限定文書差分は明示的fallbackで検査した。次actionは通常レビューfix cycleを完了として親工程へ返し、tracking同期とcommit準備の後、凍結targetに対する独立最終レビューへ進むことである。
