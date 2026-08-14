# Sub-agent実行レポート

## タスク

- 目的: T-007 最短周期の順次測距状態機械の通常レビューを行う
- タスク種別: 通常レビュー

## sub-agentを使う理由

- 理由: 実装担当とは別の`gpt-5.6-sol`、reasoning effort `high`で、複数ノード状態遷移を独立確認するため

## 対象範囲

- 対象: T-007製品コード、PlatformIO nativeテスト、直接依存、実装レポート、検証証跡

## 対象外

- 対象外: 画面表示、設定コマンド追加、実機通信、EKF、複雑な再送・完全自動復旧

## 実行コマンド

- 実行コマンド: 指定された`work-context-manager`、`review-worker`、`feedback-coding-standards-enforcer`、`report-writer`、`markdown-word-checker`を全文確認した。`git branch --show-current`、`git rev-parse HEAD`、`git status --short`、`git log --oneline -12`、`git show --stat c28461ee22bc7443a6053d0396eeb86af000a575`、`git diff-tree --name-status -r c28461ee22bc7443a6053d0396eeb86af000a575`、`git diff --check c28461ee22bc7443a6053d0396eeb86af000a575 -- include src test platformio.ini reports/T-007-sequential-ranging-controller-implementation.md`、対象コードと直接依存の全行確認、固定待機・動的確保・画面・Serial出力scan、packet識別・状態遷移・queue・Doxygen・命名scanを実行した。検証は`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe test -e native_t007 -e native_t006 -e native_t005 -e native_t004 -e native`、同`run -e m5stack-sticks3 -t clean`、同`run -e m5stack-sticks3`を固定HEADで再実行した。Markdown Word Checkerはrepositoryに`package.json`、`tools/lint/`、`cspell.config.jsonc`が存在しないためfocused/fullとも`unsupported`とし、本文を目視確認した

## 対象ファイル

- 変更または確認したファイル: T-007変更全体の`include/SequentialRangingController.h`、`src/SequentialRangingController.cpp`、`test/test_t007/test_main.cpp`、`test/test_t007/stubs/EspNowBroadcast.h`、`EspNowTransport.h`、`NtpTimeSynchronizer.h`、`Ryuw122Controller.h`、`TagMasterCoordinator.h`、`esp_timer.h`、`platformio.ini`、`reports/T-007-sequential-ranging-controller-implementation.md`を確認した。要件・設計・検証境界として`tasks-status.md`のT-007、`docs/sequential-ranging-time-sync.md`全節、`test/README`、`src/main.cpp`を確認した。T-003からT-006の直接依存として`RunMode`、`NodeStatus`、`EspNowBroadcast`、`EspNowTransport`、`TagMasterCoordinator`、`NtpTimeProtocolCodec`、`NtpTimeSynchronizer`、`Ryuw122Controller`、`SequentialRangingProtocolCodec`のheader、実装、関連test、通常reviewとfix verificationを確認した。本レビューで変更したのは予約済みの本reportの5 placeholderだけであり、未コミット`tasks-status.md`、製品、test、設計、構成、他reportは変更していない

## 指摘事項

- 指摘要約または「指摘なし」: required findingは4件である。`T007-NR-001`、severity `Medium`、origin `initial normal review`、location=`src/SequentialRangingController.cpp:693-743`、関連gate=`src/SequentialRangingController.cpp:581-590`。description=`UpdateMaster()`は非実行中の開始前に`IsSynchronizationComplete()`を確認するが、正常完了とtimeoutの共通経路`CompleteMasterRound()`は同じ確認を通らず`StartMasterRound()`を直接呼ぶ。impact=実行中ラウンドでT-004が同一sessionの新規ノードを発見し同期未完了へ戻っても、既存同期済みsnapshotで次ラウンドを開始し、NTP packetと測距制御が競合する。新規ノードを同期後に次ラウンドへ加える設計と、masterだけが全対象同期完了後に開始するgateを満たさない。evidence=`NtpTimeSynchronizer::UpdateMaster()`は既存target完了後も`DiscoverNewTargets()`し、要求中は`IsSynchronizationComplete()==false`となる一方、完了直後の直呼びにはその判定がない。repro=1 ANCHOR・1 TAGでround 1を開始し、ANCHOR同期情報を残したまま同期完了値をfalseへ変えて最終measurementを受信させると、同じ`Update()`でround 2の`RangeControl`が送信待ちになる。required action=完了経路でも同期完了を再確認し、完了なら同じ`Update()`で即時開始、未完了なら待機状態へ戻して通常gateから再開するhost testを追加する。`T007-NR-002`、severity `Medium`、origin `initial normal review`、location=`src/SequentialRangingController.cpp:297-315`と`351-354`。description=ANCHORのcontrol重複判定がround更新時にも、送信元に紐付かない単一`m_lastControlPacketSequence`より新しいpacket sequenceを要求する。impact=ラウンドsnapshot変更で対象ANCHORの直前ノードが別ANCHORからmasterへ変わる、または別ANCHORへ変わる正常経路では、新送信元の正当なsequenceが旧送信元の値以下だと新round controlを拒否し、UWB chainが開始せずmaster timeoutまで欠損する。evidence=source MACは新snapshotのturnに対して正しく検証されるが、その後`IsNewerSequence(sequence, m_lastControlPacketSequence)`がround/sourceに無関係に先行する。repro=同一sessionでANCHOR A20がround 1をA10からsequence 100で受理した後、A10失効後のround 2で先頭となったA20へmasterがsequence 5を送ると、source・round・pairが正しくてもduplicateとして拒否される。required action=新しいroundは正当なsource/turnを確認して受理し、同一round内だけsequence・pairの重複判定を行うか、sequence履歴を送信元とroundに紐付け、snapshot source変更の回帰testを追加する。`T007-NR-003`、severity `Low`、origin `initial normal review`、location=`src/SequentialRangingController.cpp:509-538`。description=フォロワーTAGの`RangeRoundComplete`処理はpacket sequenceだけを確認し、`complete.roundId`を現在の`m_roundId`と比較しない。impact=次roundの逐次forwardを受けた後に遅延した旧round完了packetが届くと、古いsummaryを新しい観測の後へ公開し、アプリケーションのround統計順を壊す。evidence=forwardの新round受理時は`m_roundId`を更新するが、完了通知側は`m_lastCompleteSequence`が0なら旧roundでも受理する。repro=フォロワーへround 2の正当なforwardを先に与え、その後round 1・nextRound 2の正当なcompleteを与えると`TryTakeCompletedRound()`がround 1を返す。required action=現在より古いroundを破棄し、現在または許容する新roundとの関係を明示してreorder/duplicate testを追加する。`T007-NR-004`、severity `Low`、origin `initial normal review`、location=`test/test_t007/test_main.cpp:240-374`および`:535-543`。description=T-007完了条件が明示する3 ANCHOR×2 TAGの`A1-T1, A1-T2, A2-T1, A2-T2, A3-T1, A3-T2`検証がなく、現testは1×1連続round、単一ANCHOR上の2 TAGと次ANCHORcontrol、masterへの2×2 packet手動投入までである。impact=3つ目のhopを含むcontrol chain、各ANCHORでの実測距開始、最終ANCHOR完了、最終結果直後の次round開始を一つの経路として回帰検知できず、明示された完了条件を満たした証拠にならない。evidence=9 testのいずれにも3 ANCHOR構成がなく、2×2 master testはANCHOR controllerを通さず順序どおりにpacketを直接注入する。repro=test一覧とfixtureのnode数を確認するとANCHORは最大2台である。required action=master、3 ANCHOR、2 TAGを接続して6件のUWB開始順、逐次公開、control優先、最終完了、round 2即時開始をPlatformIO native testで検証する。severity reclassification/erratumはない

## 結果

- 結果: verdict=`fail`。review mode=`initial normal review`、reviewer identity=`/root/t007_normal_review`で実装担当とは別であり、実装・修正を行っていない。branch=`codex/multitag-sequential-ranging`、base=`5d18194f`のfull SHAはcommit parent、commit range=`5d18194..c28461e`、current HEAD兼`reviewed_implementation_head`=`c28461ee22bc7443a6053d0396eeb86af000a575`で、技術判定はこの固定HEADだけへ適用する。coverage dispositionは、master限定開始、初回同期gate、snapshotの有効期限・一意性・ID昇順・8台制限、ANCHOR外側/TAG内側、1×1連続round、2×2順序、control優先queue、ANCHORのStart/Update/result、UWB失敗後遷移、ローカル32bitからmaster 64bit変換、逐次event・forward、最終ANCHOR完了、同期完了時の次round即時開始、follower非開始、transport idle、foreign packet非消費、source/destination/session/round/pair/identity、同一source duplicate、master reset、round timeout・欠損summary、queue満杯時の非上書き診断、固定待機・動的確保・製品出力なし、`main.cpp`境界、公開API、日本語Doxygen、命名、scope discipline、secret/securityを`checked_no_finding`とした。ラウンド間NTP gate=`checked_finding`（T007-NR-001）、snapshot変更時control sequence/turn=`checked_finding`（T007-NR-002）、follower stale round=`checked_finding`（T007-NR-003）、明示受入testとtest realism=`checked_finding`（T007-NR-004。T007-NR-001から003の回帰も欠落）、実機ESP-NOW/UWB・matching current-HEAD CI・repository固有Markdown lint=`held`、高度な再送・ACK・輻輳・完全自動復旧・画面・EKF=`not_applicable`、unexplored=なし。再実行結果は`native_t007` 9/9、T-003からT-007の全回帰48/48、M5StickS3 clean/full build、`git diff --check`が成功した。full buildは`SequentialRangingController.cpp`をcompileし、RAM 52,088 / 327,680 bytes（15.9%）、Flash 1,218,015 / 3,342,336 bytes（36.4%）。成功した既存testは4 findingのケースを実行しないため判定を上書きしない。reserved report path=`reports/T-007-sequential-ranging-controller-normal-review.md`、persistence mode=`repository_file`、`report_attestation_allowed=false`。次actionは実装担当が4 findingと対応host testを修正し、同一reviewerへfix verificationを依頼することであり、mergeは許可しない

## リスク

- 未解決のリスクまたは後続対応: required riskは`T007-NR-001`から`T007-NR-004`。heldはTAG 2台・ANCHOR 3台以上の実機ESP-NOW/UWBでの順序、時刻、control/result並行性、送信完了とpacket loss、実機master交代、queue飽和、Wi-Fi省電力ON/OFF、RYUW122遅延応答、異種endian相互運用、current-HEAD CIである。実機検証はT-008/T-009後続所有であり本レビューでは実施していない。repositoryにMarkdown lint wiringがないためfocused/fullは`unsupported`としてheldとし、手動で5 placeholder消失、表記、通常語をbacktickや引用で回避していないことを確認する。coding standards Skillのmandatory sub-agent手順は本タスクの明示的なnested agent禁止を優先して実施せず、本reviewerが追加public/private API、全宣言とcpp-only helperの日本語Doxygen、enum/class/function/member命名、ファイル名、動的確保・出力禁止を直接検査し違反なし。未コミット`tasks-status.md`と本report以外の意図的に未変更の領域は製品、test、tracking、design、構成、他reportである。unexploredはない
