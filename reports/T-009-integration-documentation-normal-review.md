# Sub-agent実行レポート

## タスク

- 目的: T-009 統合テスト、build、文書同期の通常レビューを行う
- タスク種別: 通常レビュー

## sub-agentを使う理由

- 理由: 実装担当とは別の`gpt-5.6-sol`、reasoning effort `high`で、統合証跡と文書整合を独立確認するため

## 対象範囲

- 対象: T-009製品修正、統合テスト、全検証結果、設計書、機能一覧、test README

## 対象外

- 対象外: 実機無線試験、EKF、座標計算、高度な再送・完全自動復旧

## 実行コマンド

- 実行コマンド: `C:\Users\taiga\.platformio\penv\Scripts\platformio.exe test -e native -e native_t004 -e native_t005 -e native_t006 -e native_t007 -e native_t008 -e native_t009`（60/60成功）、`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe run -e m5stack-sticks3 -t clean`（成功）、`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe run -e m5stack-sticks3`（成功）、`git diff --check`（成功）、`git diff`・`rg`・`Get-Content`によるT-009差分、production直接依存、Doxygen、命名、packet size、固定長queue、callback責務、`main.cpp`境界、文書、placeholderの静的確認。最初のbare `platformio test ...`は実行ファイルが`PATH`にないため起動できず、同じPlatformIO executableの絶対pathで再実行して成功した。repositoryに`tools/lint/`、`package.json`、`cspell.config.jsonc`がないため、Markdown focused/full lintは`unsupported`でありpass証跡にはしていない。

## 対象ファイル

- 変更または確認したファイル: branch `codex/multitag-sequential-ranging`、base/current Git HEAD `6ce336551710ddd9707433006ed818758d099427`と未コミットT-009差分を固定対象とした。`tasks-status.md`の親開始差分と本通常レビューレポート自身はimplementation対象から除外した。T-009変更の`include/NtpTimeSynchronizer.h`、`src/NtpTimeSynchronizer.cpp`、`platformio.ini`、`test/test_t004/test_main.cpp`、`test/test_t009/test_main.cpp`、`test/test_t009/stubs/`、`test/README`、`docs/sequential-ranging-time-sync.md`、`docs/feature-list.md`、`reports/T-009-integration-documentation-implementation.md`を確認した。要件・追跡として`tasks-status.md`のT-009、`phases-status.md`、`docs/preferences-commands.md`を確認し、production直接依存・文書裏付けとして`include/`・`src/`の`NodeStatus`、`TagMasterCoordinator`、`NtpTimeProtocolCodec`、`NtpTimeSynchronizer`、`SequentialRangingProtocolCodec`、`SequentialRangingController`、`EspNowTransport`、`EspNowBroadcast`、`Ryuw122Controller`、`SequentialRangingDisplay`、`ConfigPreference`、`ConfigRuntime`、`PreferenceCommands`、`main.cpp`とT-003からT-008のtestを確認した。T-009 binaryはproductionの`NodeStatus.cpp`、`TagMasterCoordinator.cpp`、`NtpTimeProtocolCodec.cpp`、`NtpTimeSynchronizer.cpp`、`SequentialRangingProtocolCodec.cpp`、`SequentialRangingController.cpp`を直接compile/linkし、stubは実行時設定、NodeStatus保持、transport、時刻・乱数、fake UWBの外部境界に限定され、本質的な選出・同期・codec・逐次制御logicを置換していない。

## 指摘事項

- 指摘要約または「指摘なし」: **T009-NR-001 Medium**（origin: normal review、location: `docs/sequential-ranging-time-sync.md:516-585`）。説明: packet形式を実装同期した節が完全なwire contractと一致していない。`NtpSyncResponsePacket`の説明は`receiveTimestampAvailable`と`powerSaveEnabled`を、`RangeControlPacket`の説明は`pairSequence`と`tagIndex`を含めず、逐次結果は実在しない`RangeMeasurementWireResult`という部分構造名で示して、実装の`RangeMeasurementPacket`が持つheader、round/pair/master識別、件数・index、両ノード識別、master変換時刻、同期品質等を欠落させている。影響: 設計をwire相互運用の根拠にする実装者が、不完全または異なるpacketを作る可能性があり、T-009完了条件「設計と実装に既知の不一致がない」を満たさない。再現・証拠: 該当節を`include/NtpTimeProtocolCodec.h`の`NtpSyncResponsePacket`、`include/SequentialRangingProtocolCodec.h`の`RangeControlPacket`・`RangeMeasurementPacket`とフィールド単位で比較する。required action: 実在する構造体名を使い、各packetの全wire fieldまたは明示した完全な参照先を記載し、概念的抜粋を残す場合はwire schemaではないことを明記する。**T009-NR-002 Low**（origin: coding-standards/documentation review、location: `include/NtpTimeProtocolCodec.h:68,176`、`include/NtpTimeSynchronizer.h:115,300,327`）。説明: T009-IF-001で`NtpSyncCommit`を全非master targetへ拡張した後も、公開型・parameter・ローカル変換・commit処理/送信のDoxygenが「フォロワーTAG」限定のままである。影響: ANCHORがcommitを受理し`TryConvertLocalTimeToMaster()`を使う正規経路をAPI利用者が判別できず、実装・設計・API文書が不一致になる。再現・証拠: `rg -n "フォロワーTAGへ送る同期確定|対象フォロワーTAG ID|フォロワーTAG自身|フォロワーの変換|フォロワーTAGへ採用済み" include/NtpTimeProtocolCodec.h include/NtpTimeSynchronizer.h`で5箇所を確認できる一方、`src/NtpTimeSynchronizer.cpp`の`FinalizeCurrentTarget()`は全targetを`commitPending=true`とし、`HandleCommit()`はTAG/ANCHOR双方の正当なcommitを受理する。required action: 5箇所を「全非マスターノード」「対象ノード」「自ノード」へ更新し、ANCHORとフォロワーTAG双方の契約を正確に表す。severity reclassificationなし。

## 結果

- 結果: `fail`。review modeはinitial normal review、reviewer identityは`/root/t009_normal_review`で実装担当とは別、reviewed implementation identityは`codex/multitag-sequential-ranging`の`6ce336551710ddd9707433006ed818758d099427` + 本レビュー開始時に固定した未コミットT-009差分である。T009-IF-001について、masterがTAG・ANCHOR全4 targetへ3 sample後のcommitを送り、各非masterがcommit受信後に同期完了すること、source/destination/session/target/channel検証を保持すること、ANCHORの通常`RangeControl`受理とローカル時刻変換を確認した。統合testは3 ANCHOR×2 TAGの`A1-T1, A1-T2, A2-T1, A2-T2, A3-T1, A3-T2`、1件ずつの逐次公開、master時刻変換、round完了、2,000,000us deadlineを越える基本timeout、低ID master参加によるreset、30,001ms経過後の旧master失効、新session再同期後の測距再開を妥当にassertする。全native 60/60成功。M5StickS3 clean/full build成功、RAM 68,112 / 327,680 bytes（20.8%）、Flash 1,232,811 / 3,342,336 bytes（36.9%）。Doxygen存在、日本語`@brief`、引数/戻り値、enum/class/function/member/file命名、全wire packetの250 bytes以下`static_assert`、production callback内の動的確保・画面/Serial/UWB処理なし、測距・結果・送信queueの固定長、`main.cpp`のcomposition/update境界、OS専用test script追加なしは`checked_no_finding`。requirements/design/documentation accuracyは上記2件で`checked_finding`。security/secret、外部API・設定破壊は`not_applicable`。実機と異種endianは`held`。CIは未コミットtargetのためmatching current-HEAD runを要求できず`not_applicable`。`unexplored`はない。required findingがあるためT-009完了判定前に両findingを修正し、Markdown目視検査、`git diff --check`、全native test、M5StickS3 clean/full buildを再検証する。reserved report pathは`reports/T-009-integration-documentation-normal-review.md`、`report_attestation_allowed: false`、mergeは行わない。

## リスク

- 未解決のリスクまたは後続対応: required findingはT009-NR-001 MediumとT009-NR-002 Low。heldはTAG 2台・ANCHOR 3台の実機ESP-NOW/UWB順序、実機packet loss・queue飽和・clock drift・Wi-Fi省電力差・画面視認性・NT-Shell同時操作、異種endian相互運用、M5Stack系hardwareへの実移植であり、本ホスト/M5StickS3 build gateを阻害しない。EKF、座標計算、アプリケーションACK、複雑な再送、輻輳制御、完全自動復旧、同期期限による自動`SynchronizationExpired`遷移、周期的再同期は文書上も未実装として区別されている。Markdown lintはrepository wiring不足のため`unsupported`であり、設定を変更せずplaceholder・全角空白・不自然なbacktick/quote回避を検索・目視した。remaining riskは実機radio/timingと異種endianをhost testで代替できないこと、およびdocumentation fixes後に再reviewが必要なことである。
