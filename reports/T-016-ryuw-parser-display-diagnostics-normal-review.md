# Sub-agent実行レポート

## タスク

- 目的: T-016のRYUW122応答解析、FAIL分離表示、診断ビルド、実機ログfollow-up修正を通常レビューする。
- タスク種別: 通常レビュー

## sub-agentを使う理由

- 理由: 実装担当とは別のレビュー担当が、大幅変更と実機修正を独立した視点で確認するため。

## 対象範囲

- 対象: 初回通常レビュー。branch `codex/display-three-nodes-tag-results`、origin既定branch `master`、base／開始HEAD `657fe73290c1e3344d20fca5439ca68e051d960b`と、レビュー開始時の全未コミット変更（本reportを除く34項目、snapshot SHA-256 `ac49de52d13e5cb72e83eeacb604fde367dff244ebd0f5f70be1ca602adb3731`）をreviewed implementation snapshotとした。T-016のparser 4／5 field、数値／空白付き`cm`、任意RSSI、`+ERR`、成功・失敗各5件とNodeStatus 5件、last-success保持、duration、通常NT-Shell／診断build、固定長診断queue、測距・表示task分離、Busy保留、TAG `TAG_SEND=1,T`、ANCHOR `ANCHOR_SEND=...,1,A`、`+OK`別timeout code、117-byte wire互換、design／docs／tests／tracking、全変更ファイルと直接依存を対象にした。

## 対象外

- 対象外: 実装修正、Git add／commit／push／branch／PR／merge、task／phaseや本report以外の編集、独立最終レビュー、nested Codex／sub-agent、development-orchestrator再入。実機への追加upload、Serial操作、既存`.pio/libdeps`の変更も行っていない。

## 実行コマンド

- 実行コマンド: `git status --short --branch`、`git rev-parse HEAD`、`git branch --show-current`、`git symbolic-ref refs/remotes/origin/HEAD`、snapshot SHA-256再計算、`git diff --stat`／`--numstat`／changed-file別diff、`rg`と行番号付き読取による要件・Doxygen・命名・API・状態遷移・queue／task concurrency・wire・直接依存確認、`%USERPROFILE%\.platformio\penv\Scripts\platformio.exe test -e native_t005 -e native_t007 -e native_t008 -e native_t009 -e native_t015`（69/69成功）、全native 9環境（104/104成功）、通常／診断envのcleanとfull build、ELFの`nm`相当symbol確認と診断format文字列確認、`git diff --check`（成功）、Markdown設定・相対link確認を実行した。repositoryに`package.json`、`tools/lint/*`、`cspell.config.jsonc`がなくMarkdown専用lintは`unsupported`であり、`git diff --check`、link存在確認、用語検索で補完した。

## 対象ファイル

- 変更または確認したファイル: 変更全体の`docs/current-class-architecture.md`、`docs/feature-list.md`、`docs/sequential-ranging-time-sync.md`、`include/BuildOptions.h`、`include/RangingDisplayTaskController.h`、`include/Ryuw122Controller.h`、`include/Ryuw122Initializer.h`、`include/Ryuw122ResponseParser.h`、`include/SequentialRangingController.h`、`include/SequentialRangingDisplay.h`、`phases-status.md`、`platformio.ini`、`reports/T-016-ryuw-parser-display-diagnostics-implementation.md`、`src/RangingDisplayTaskController.cpp`、`src/Ryuw122Controller.cpp`、`src/Ryuw122Initializer.cpp`、`src/Ryuw122ResponseParser.cpp`、`src/SequentialRangingController.cpp`、`src/SequentialRangingDisplay.cpp`、`src/main.cpp`、`tasks-status.md`、`test/README`、`test/test_t005/stubs/RYUW122.h`、`test/test_t005/test_main.cpp`、`test/test_t007/stubs/Ryuw122Controller.h`、`test/test_t007/test_main.cpp`、`test/test_t008/stubs/SequentialRangingDisplay.h`、`test/test_t008/test_main.cpp`、`test/test_t009/stubs/Ryuw122Controller.h`、`test/test_t015/stubs/Arduino.h`、`test/test_t015/stubs/SequentialRangingController.h`、`test/test_t015/stubs/TaskTestRuntime.h`、`test/test_t015/stubs/freertos/queue.h`、`test/test_t015/test_main.cpp`を確認した。直接依存として`ConfigRuntime`、`EspNowTransport`／`EspNowBroadcast`／`EspNowReceiveQueueTerminator`、`NtpTimeSynchronizer`、`TagMasterCoordinator`、`SequentialRangingProtocolCodec`、`NodeStatus`、FreeRTOS queue／task API、M5Canvas／Print、RYUW122 1.0.1 header／実装を読取り、`.pio/libdeps`は変更していない。本reportは構造と既存textを維持しplaceholderだけを記入した。

## 指摘事項

- 指摘要約または「指摘なし」: `T016-NR-001` Medium、origin=`initial normal review`、location=`src/SequentialRangingController.cpp:105-114,205-215,378-390,626-640`および`src/Ryuw122Controller.cpp:522-536`。master／session変更で`ResetSessionState()`したcycleに旧測距結果がreadyとなり、新masterのcontrolも処理されると、`m_anchorRangingStarted=false`の新経路が`IsBusy()`でreturnする一方、Busy原因の`m_hasResult`は後段の`TryTakeResult()`を通らないため、ANCHORが`AnchorRanging`から進めなくなる。影響は新sessionの測距command・結果送信が恒久停止し、master roundがtimeoutし続けること。旧session結果を新controlのBusy待ち前に明示的に破棄するかsession／generationへ関連付けて消費し、この交差順序のproduction状態遷移testを追加することが必須。`T016-NR-002` Low、origin=`initial normal review`、location=`include/SequentialRangingDisplay.h:229`、`test/test_t008/stubs/SequentialRangingDisplay.h:222`、`test/test_t008/test_main.cpp:468`。実装・要件はNodeStatus 5件なのにDoxygenは「先頭3件」、test名は`TestEightAnchorResultsAndThreeNodesFitScreen`のままである。影響は公開契約とtest意図の誤読、coding-standards／文書同期証拠の不正確化。5件契約へDoxygenとtest名を同期すること。`T016-NR-003` Low、origin=`initial normal review`、location=`reports/T-016-ryuw-parser-display-diagnostics-implementation.md:22,34,38`。現snapshotは`native_t005` 23件、`native_t007` 14件、全native 104件、Flash通常1,238,407 byte／診断1,228,179 byteで、さらにユーザー提示のCOM7／COM10実機follow-upが完了しているが、implementation reportは22件／13件／102件、旧Flash値、実機採取を次actionとして記載する。影響は最終snapshotの耐久証拠と実態が一致しないこと。Busy follow-up後のtest／build／実機証拠を同reportまたは明確なfollow-up reportへ記録し、対象snapshotを識別できるようにすること。

## 結果

- 結果: verdict=`fail`。`T016-NR-001` MediumとLow 2件のrequired findingがある。要件・設計適合=`checked_finding`、correctness／edge case=`checked_finding`、scope discipline=`checked_no_finding`、全changed files／直接依存=`checked_finding`、API／data／configuration／workflow／wire互換=`checked_no_finding`、error handling／failure diagnostics=`checked_no_finding`、security／secret handling=`not_applicable`、tests／validation=`checked_finding`、report／tracking／documentation accuracy=`checked_finding`、regression／maintainability=`checked_finding`。parser 4／5 field、`cm`、RSSI、`+ERR`、TAG／ANCHOR payload、`+OK` timeout code、Busy通常経路、成功・失敗各5件、last-success、duration、NodeStatus 5件、task／Serial境界、117-byte wireは静的確認と104/104 native testで成立した。通常／診断clean・full buildは成功し、通常RAM 69,200／327,680 byte・Flash 1,238,407／3,342,336 byte、診断RAM 69,176／327,680 byte・Flash 1,228,179／3,342,336 byte。通常ELFは`NtShell::Start()` 1件・診断format 0件、診断ELFは`NtShell::Start()` 0件・診断format 1件。レビュー担当は実装に参加していないCodex built-in normal reviewer `/root/t016_normal_review`で、本report以外を編集していない。reviewed implementation headはcommitではなく、HEAD `657fe73290c1e3344d20fca5439ca68e051d960b`と未コミットsnapshot SHA-256 `ac49de52d13e5cb72e83eeacb604fde367dff244ebd0f5f70be1ca602adb3731`の組である。reserved report pathは`reports/T-016-ryuw-parser-display-diagnostics-normal-review.md`、`report_attestation_allowed=false`、report attestation headはnull。次actionは3 findingの修正と同一identityを更新したfix verificationであり、mergeは許可しない。

## リスク

- 未解決のリスクまたは後続対応: heldとして、ユーザー提示の実機証拠は修正前COM7の多数`START`＋300ms`TIMEOUT`、COM10のTAG ID0／channel 4／power save off、両端修正版upload後COM7の連続`OK`、340～950mm、主に64～66ms・最短56ms、`START`／`TIMEOUT`／`ERR`／`PARSE`なしを支持する一方、reviewer自身は実機を再操作していない。TAG 2台＋ANCHOR 3台以上の順序、packet loss、診断二段queue飽和、135×240実画面の5＋5＋5件視認性と通常版`SH`表示、NT-Shell同時操作、修正版実機での`ERR`／`PARSE`／`START`／timeout code 0／1各失敗経路、master交代・途中参加、NTP drift、power save差、GPIO8電気特性は未確認でheld。current snapshotは未コミットのため一致するCI runはなくheld。Markdown専用lintはrepository wiring不足で`unsupported`、coding-standards Skillの通常sub-agent経路は今回の明示的なnested禁止により使わず、Doxygen・命名・公開APIを直接検査した。unexploredはなし。
