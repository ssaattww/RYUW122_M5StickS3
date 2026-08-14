# Sub-agent実行レポート

## タスク

- 目的: T-008 アプリケーション統合と逐次表示の通常レビューを行う
- タスク種別: 通常レビュー

## sub-agentを使う理由

- 理由: 実装担当とは別の`gpt-5.6-sol`、reasoning effort `high`で、更新順と画面統合を独立確認するため

## 対象範囲

- 対象: T-008製品コード、既存画面・NT-Shell境界、回帰テスト、実装レポート、検証証跡

## 対象外

- 対象外: EKF、座標計算、実機無線調整、高度な再送・完全自動復旧

## 実行コマンド

- 実行コマンド: `work-context-manager`、`review-worker`、`feedback-coding-standards-enforcer`、`report-writer`、`markdown-word-checker`を全文確認した。`git status --short --branch`、`git rev-parse HEAD`、`git diff --stat`、`git diff --name-only`、`git diff -- src/main.cpp`、`git show HEAD:src/main.cpp`、対象ファイルのSHA-256取得、`rg`と全行読取りによるcomposition lifetime、Begin失敗、共有受信FIFO、更新順、NTP gate、RYUW122順序、表示event、Canvas範囲、Serial出力、動的確保、Doxygen、命名、NT-Shell／NVS設定経路の静的検査、`git diff --check`を実行した。検証は`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe test -e native -e native_t004 -e native_t005 -e native_t006 -e native_t007`、同`run -e m5stack-sticks3 --target clean`、同`run -e m5stack-sticks3`を再実行した。repositoryに`package.json`、`tools/lint/`、`cspell.config.jsonc`がないためMarkdown focused／full wording lintは`unsupported`とし、本reportを手動確認した

## 対象ファイル

- 変更または確認したファイル: 技術差分`src/main.cpp`、`include/SequentialRangingDisplay.h`、`src/SequentialRangingDisplay.cpp`を全行確認した。要件・設計・証跡として`tasks-status.md`のT-008、`phases-status.md`、`docs/sequential-ranging-time-sync.md`のクラス構成、逐次結果、TAG／ANCHOR状態機械、timeout、時刻品質、`main.cpp`境界、規約、検証節、`test/README`、`platformio.ini`、`reports/T-008-application-integration-implementation.md`を確認した。直接依存として`EspNowTransport`、`EspNowBroadcast`、`NodeStatus`、`TagMasterCoordinator`、`NtpTimeSynchronizer`、`Ryuw122Controller`、`SequentialRangingProtocolCodec`、`SequentialRangingController`、`ConfigRuntime`、`ConfigPreference`、`PreferenceCommands`、`NtShell`のheader／実装、T-003からT-007の関連testと通常review／fix verificationを確認した。本レビューで変更したのは予約済みの本reportの5 placeholderだけであり、親所有の`tasks-status.md`開始差分、製品、test、設計、構成、実装report、他reportを変更していない

## 指摘事項

- 指摘要約または「指摘なし」: required findingは3件である。

  - `T008-NR-001`、severity `Medium`、origin `initial normal review`、location=`src/SequentialRangingDisplay.cpp:31-45`。description=`SequentialRangingController::ProcessReceivedPackets()`は共有transportに連続到着した全packetを1回の`Update()`で処理し、最大64件のmeasurement FIFOと4件のround FIFOへeventを追加できるが、displayの1回の`Update()`は各FIFOから1件しか取り出さない。impact=burst後は最新値ではなく古い値を1 loopにつき1件ずつ全画面再描画し、表示がevent数ぶん遅延する。producerが継続して1 loopあたり複数eventを追加する条件では表示consumerが追いつかず、controller側FIFO overflowと描画による次測距遅延を誘発する。repro=controller FIFOへmeasurementを2件、summaryを2件追加してdisplayを1回更新すると、それぞれ1件が未取得で残り、次回も`changed=true`となって古いeventごとに`pushSprite()`が必要になる。required action=`TryTakeMeasurement()`と`TryTakeCompletedRound()`をそれぞれ空になるまでdrainし、固定メンバーには最後の1件だけを保持して1回の再描画要求へ集約する。複数event burst、空queue、measurementのみ、summaryのみを表示testで検証する

  - `T008-NR-002`、severity `Medium`、origin `initial normal review`、location=`src/main.cpp:130-151`、再描画経路=`src/main.cpp:184-190`と`src/SequentialRangingDisplay.cpp:68-75`。description=RYUW122またはESP-NOWのBegin失敗文字列はsetup時にCanvasへ一度だけ直接描かれ、失敗状態を保持する表示modelがない。後続のNodeStatus受信、controller状態変更、またはBtnA操作で通常再描画が発生すると、`SequentialRangingDisplay::Draw()`がstatus barより下を黒で消去して失敗文字列を復元しない。impact=初期化失敗中にも画面が正常な`SEQ`表示へ見え、利用者が測距不能の原因を確認できなくなる。repro=RYUW122 Beginを失敗させESP-NOWを開始し、remote NodeStatusを1件受信するとdisplay `Update()`がtrueとなり、次のDrawで`RYUW122: ...`が消える。ESP-NOW失敗もBtnA再描画で同様に消える。required action=Begin結果を永続的な固定表示状態へ保存して毎回Drawするか、displayへ初期化healthを注入し、成功するまで通常描画で消えないことをRYUW失敗、transport失敗、broadcast失敗、ボタン／NodeStatus再描画で検証する

  - `T008-NR-003`、severity `Low`、origin `initial normal review`、location=`include/SequentialRangingDisplay.h:93-98`と`src/SequentialRangingDisplay.cpp:53-57,79-135`。description=controllerはmaster session変更時に未取得event FIFOを消去するが、displayがすでに取得した前sessionのmeasurement／summaryと`m_hasMeasurement`／`m_hasSummary`を消去する識別子または遷移処理がない。先頭行の時刻品質も現在の同期状態ではなく前sessionの最新measurementから選ぶ。impact=master交代後に状態が`SYNC`または`WAIT`へ戻っても、旧round、距離、summaryと`Q:SYNC`が新しい役割／状態の下へ残り、画面上のsession品質を誤表示する。repro=同期済みroundを1件表示後、より小さいTAG参加などでmaster sessionを変更するとcontroller queueはresetされるが、displayはstateだけ更新し旧measurement／summaryを描き続ける。required action=現在session／master identityまたは明示的なcontroller reset generationをdisplayへ公開し、変更時に保持済みeventと品質をclearする。master→follower、follower→master、master session更新の表示testを追加する。severity reclassificationとerratumはない

## 結果

- 結果: verdict=`fail`。review mode=`initial normal review`、reviewer identity=`/root/t008_normal_review`で実装担当とは別であり、実装・fixを行っていない。branch=`codex/multitag-sequential-ranging`、base/current HEAD兼`reviewed_implementation_head`=`7771fb196012607479207bbdd907b4a48db476c8`、reviewed target=`同HEAD + 未コミットT-008技術差分`である。対象identityは`src/main.cpp` SHA-256 `1E8A314D4A118C18DB8895EC50B91355B19812F1A0C0346D8DE317FBFA897D30`、`include/SequentialRangingDisplay.h` `D31BE032E1EA59235122052C7A87A091CE72D2A6AF62C8B48FD7351F9341E742`、`src/SequentialRangingDisplay.cpp` `E2FD26D0249934FE454B2F439D3744C395F2E765677AE9F80CAE067ABC6A4428`で、技術判定はこのHEADと3ファイルidentityの組へだけ適用する。coverage dispositionは、要件・設計適合、正常系correctness、display event consumer、Begin failure diagnostics、master session表示reset、test adequacy=`checked_finding`（T008-NR-001から003）、composition lifetime／global construction順、RYUW122→transport→broadcast→coordinator→controller Begin gate、loopのtransport→broadcast→coordinator→NTP→RYUW122→controller→display順、共有受信FIFOの非対象packet保持、NTP完了gate、次制御が表示より先にqueueされる境界、mainのNTP計算／packet解析／二重loop不在、固定長表示状態／新規動的確保なし、M/F/A role、controller state、result status、距離、UWB RSSI、時間、summary件数／欠損／timeout、ID付きstatus bar、NodeStatus headerと2行、Canvas縦範囲／status bar非重複／content clear／単一push経路、measurement／summary／NodeStatus／stateのrefresh trigger、新規Serial測距出力なし、既存NT-Shell task／cursor／preferences UI、`wifi_power_save`の既定falseと`pref get bool`／`pref set bool`／`pref list`経路、API／data／configuration互換、scope、security／secret、日本語Doxygen、引数順`@param`、非void `@return`、UpperCamelCase、`m_` lowerCamelCase、主要classとファイル名一致=`checked_no_finding`。実機表示視認性、ESP-NOW／UWB、queue飽和、NT-Shell同時操作、Wi-Fi省電力差、matching current-HEAD CI、repository固有Markdown lint=`held`、EKF／座標計算／高度な再送／完全自動復旧=`not_applicable`、unexplored=なし。再実行結果はPlatformIO native T-003 5件、T-004 13件、T-005 12件、T-006 9件、T-007 13件の全52/52成功、M5StickS3 clean/full build成功、RAM 68,096 / 327,680 bytes（20.8%）、Flash 1,232,483 / 3,342,336 bytes（36.9%）、`git diff --check`成功である。成功した既存testはT-008 displayをcompile／実行せず、3 findingを上書きしない。reserved report path=`reports/T-008-application-integration-normal-review.md`、persistence mode=`repository_file`、`report_attestation_allowed=false`。次actionは実装担当が3 findingを修正し、表示testを追加して同一reviewerへfix verificationを依頼することである。stage／commit／push／PR／mergeは実施していない

## リスク

- 未解決のリスクまたは後続対応: required riskは`T008-NR-001`から`T008-NR-003`。heldはTAG 2台・ANCHOR 3台以上の実機ESP-NOW/UWB逐次測距、実機master交代と初期化失敗、packet loss、queue飽和、Wi-Fi省電力ON/OFF、timeout／欠損summary、M5StickS3実画面での文字収まり・視認性・ちらつき、NT-Shell同時操作、matching current-HEAD CIである。Canvasの135x240想定上、現在の固定Y座標はstatus barと重ならず2 node行まで縦範囲内だが、実フォント、画面rotation、最大桁数の横方向収まりは実機で確認する。実行中BtnAによるmode変更は既存UI経路だが、RYUW122、coordinator、controllerを再初期化しない動的mode変更は設計上の初期対象外としてheldを維持する。repository固有Markdown lintはwiring不在でfocused／fullとも`unsupported`のためheldとし、手動で5 placeholder消失、構造、用語、通常語のbacktick／引用回避なしを確認する。coding standards Skillのmandatory sub-agent手順は明示されたnested agent禁止を優先して実施せず、本reviewerが直接検査し追加違反なし。意図的に未変更の範囲は親所有tracking開始差分、製品、test、設計、構成、実装report、他reportである。unexploredはない
