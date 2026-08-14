# Sub-agent実行レポート

## タスク

- 目的: T-005 RYUW122非同期測距APIの通常レビューを行う
- タスク種別: 通常レビュー

## sub-agentを使う理由

- 理由: 実装担当とは別の`gpt-5.6-sol`、reasoning effort `high`で、UART解析と遅延応答境界を独立確認するため

## 対象範囲

- 対象: T-005製品コード、PlatformIO nativeテスト、RYUW122参照実装、実装レポート、検証証跡

## 対象外

- 対象外: ESP-NOW測距packet、複数TAG順次制御、実機通信、高度な再送・完全自動復旧

## 実行コマンド

- 実行コマンド: `C:\Users\taiga\.platformio\penv\Scripts\platformio.exe test -e native_t005`（8/8成功）、同`test -e native_t004`（13/13成功）、同`test -e native`（5/5成功）、同`run -e m5stack-sticks3 -t clean`（成功）、同`run -e m5stack-sticks3`（成功、RAM 52,088 / 327,680 bytes、Flash 1,217,379 / 3,342,336 bytes）、`git diff --check`（成功）。最初の`platformio.exe test -e native_t005`はshellの`PATH`に実行ファイルがなく起動できなかったため、同じPlatformIO Test Runnerを絶対pathで再実行した。`git rev-parse HEAD`、`git branch --show-current`、`git status --short --untracked-files=all`、対象ファイルのSHA-256でも開始時と終了前のidentityを確認した。Markdown検査は`tools/lint/`、`package.json`、`cspell.config.jsonc`がすべて存在せず、focused/fullとも実行経路なしの`unsupported`と判定した。

## 対象ファイル

- 変更または確認したファイル: 変更全体の`include/Ryuw122Controller.h`、`src/Ryuw122Controller.cpp`、`platformio.ini`、`test/test_t005/test_main.cpp`、`test/test_t005/stubs/Arduino.h`、`test/test_t005/stubs/ConfigRuntime.h`、`test/test_t005/stubs/RYUW122.h`、`tasks-status.md`、`reports/T-005-ryuw122-async-ranging-implementation.md`、本予約report。要件・直接依存として`docs/sequential-ranging-time-sync.md`のRyuw122Controller、ANCHOR状態機械、timeout、`main.cpp`境界、coding規約、検証方針、実装順序、`tasks-status.md`のT-005全文、`test/README`、`src/main.cpp`、base版のRyuw122Controller、`.pio/libdeps/m5stack-sticks3/RYUW122/RYUW122.h`と`RYUW122.cpp`をread-only確認した。

## 指摘事項

- 指摘要約または「指摘なし」: `T005-NR-001`、Medium、origin=`initial normal review`。location=`src/Ryuw122Controller.cpp:360`のUART全行読取、`:424-445`の`ProcessLine()`、特に`:426-429`の単一`m_hasResponse`早期return、および`test/test_t005/test_main.cpp:413-443`の単一応答だけの実機adapter test。description=実機portはcontrollerが`TryTakeResponse()`する前にUARTを最後まで読み切る一方、解析済み応答を1件しか保持しないため、同一`Update()`で2件目以降の測距応答を無条件に破棄する。impact=timeout後の古い応答や異なるTAG応答に続いてactive TAG応答が同じUART burstへ入ると、古い応答だけがcontrollerへ渡されてTAG不一致で無視され、正しい応答は失われて300ms後に誤ったtimeout結果となる。これは多応答、異なるTAG応答、遅延応答の次要求への誤帰属防止契約を満たさず、順次測距を不要に停止・遅延させる。evidence=`Ryuw122HardwarePort::Update()`は各行で`ProcessLine()`を呼ぶが、1件目で`m_hasResponse=true`となった後は後続行を`:426`で捨て、外側controllerの`:558-560`による取得はport更新完了後である。repro=ANCHOR modeで`T0000002`を開始し、1回のcontroller `Update()`前にUARTへ`+ANCHOR_RCV=T0000003,0,,10,-80\r\n+ANCHOR_RCV=T0000002,0,,42,-77\r\n`を連続投入する。1件目だけ取得されてaddress不一致で無視され、2件目は取得不能のまま期限で`TimedOut`になる。required action=固定長FIFOなどで到着順の複数解析応答を保持し、少なくとも同一更新内の異なるTAG応答後のactive応答、timeout後の古い応答後のactive応答、FIFO満杯時の明示的で安全な方針をPlatformIO Test Runnerで再現検証し、正しいactive結果が成功として一度だけ公開されるよう修正する。ほかのrequired findingはなし。severity reclassification/erratumなし。

## 結果

- 結果: verdict=`fail`。review mode=`initial normal review`、reviewer identity=`/root/t005_normal_review`（実装担当とは別、実装・修正を行っていない）、branch=`codex/multitag-sequential-ranging`、base/current HEAD=`933901ebff4b7792c3c5033ea51e644c939d6cbe`、reviewed target=`同HEAD + 未コミットT-005 worktree snapshot`。製品・test・config・tracking・実装reportの対象path setとSHA-256を終了前にも固定確認し、review中の対象変更なし。required coverageは、要件/設計整合=`checked_finding`、正確性・edge case=`checked_finding`、scope discipline=`checked_no_finding`、全変更file・直接依存=`checked_finding`、API/config/互換性=`checked_no_finding`、error handling=`checked_finding`、security/secret=`not_applicable`、test/validation adequacy=`checked_finding`、current-HEAD CI=`held`、report/tracking/docs=`checked_no_finding`、回帰/保守性=`checked_finding`。個別にはG7 TX/G1 RX/115200、既存mode/network/address初期化経路、blocking測距API不使用、Start/Update/TryTake/Busy、UART commandと実形式、成功/失敗/300ms timeout、32bit wrap、drain中と結果未取得busy、cm→mm overflow防止、RSSI、開始/完了timestamp、TAG role拒否、partial line、overflow行破棄、ownership/lifetime、`main.cpp`境界、製品出力なし、日本語Doxygenと命名を`checked_no_finding`、多応答・異なるTAG・遅延応答連続時を`checked_finding`とした。コーディング規約Skillのsub-agent必須手順は今回の明示的なnested agent禁止により実施せず、同じ項目を本reviewerが直接検査して違反なし。`unexplored=none`。次actionは`T005-NR-001`修正、回帰test追加、同一reviewerによるfix verification。reserved report path=`reports/T-005-ryuw122-async-ranging-normal-review.md`、`report_attestation_allowed=false`、commit/stage/push/mergeなし。

## リスク

- 未解決のリスクまたは後続対応: required riskは`T005-NR-001`。heldは、実機RYUW122 1.0.1での実応答形式・応答時刻・UART送信失敗挙動が未検証であること、設計どおり有限300ms drainを採るため300msを超えて到着する同一TAG応答は将来要求へ誤帰属し得ること、未コミットworktreeなのでmatching current-HEAD CIが存在しないこと、repositoryにMarkdown lint wiringがなく本reportのfocused/full用語検査が`unsupported`であること。これら実機・CI・lintのheldは今回確認したコード欠陥を成功扱いせず、また追加findingにも置換しない。partial line、96-byte超過行、32bit wrap、距離変換上限はcode pathを確認したが専用native回帰testはなく、`T005-NR-001`修正時に併せて追加すると残存回帰リスクを下げられる。意図的に未変更なのは製品/test/config/tracking/設計/他report/参照libraryであり、mergeは許可しない。
