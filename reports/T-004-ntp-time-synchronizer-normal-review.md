# Sub-agent実行レポート

## タスク

- 目的: T-004 NTP四時刻同期とマスター時刻変換の通常レビューを行う
- タスク種別: 通常レビュー

## sub-agentを使う理由

- 理由: 実装担当とは別の`gpt-5.6-sol`、reasoning effort `high`で、正常系と後続測距処理への接続境界を独立確認するため

## 対象範囲

- 対象: T-004製品コード、PlatformIO nativeテスト、直接依存、実装レポート、検証証跡

## 対象外

- 対象外: UWB測距、順次測距制御、実機通信、高度な再送・clock drift補正

## 実行コマンド

- 実行コマンド: `git rev-parse HEAD`、`git branch --show-current`、`git status --short --branch`、`git diff --stat`、`git diff --name-status`、`git diff --check`、`rg`と`Get-Content`による要件・設計・全差分・直接依存・日本語Doxygen・命名・動的確保/出力の検査、`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe test -e native_t004`、`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe test -e native`、`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe run -e m5stack-sticks3 -t clean`、`C:\Users\taiga\.platformio\penv\Scripts\platformio.exe run -e m5stack-sticks3`。実装差分と全untracked実装ファイルからレビュー対象fingerprintを算出した

## 対象ファイル

- 変更または確認したファイル: `tasks-status.md`のT-004、`phases-status.md`、`docs/sequential-ranging-time-sync.md`のtransport、NTP、packet、時刻品質、`main.cpp`境界、コーディング規約、検証方針、`test/README`、`platformio.ini`、`include/NtpTimeProtocolCodec.h`、`src/NtpTimeProtocolCodec.cpp`、`include/NtpTimeSynchronizer.h`、`src/NtpTimeSynchronizer.cpp`、`include/EspNowTransport.h`、`src/EspNowTransport.cpp`、`src/main.cpp`、`test/test_t004/test_main.cpp`、`test/test_t004/stubs/*`、`include/NodeStatus.h`、`include/RunMode.h`、`include/TagMasterCoordinator.h`、`src/TagMasterCoordinator.cpp`、`include/EspNowBroadcast.h`、`src/EspNowBroadcast.cpp`、`reports/T-004-ntp-time-synchronizer-implementation.md`、本レポート。製品コード、test、trackingは変更していない

## 指摘事項

- 指摘要約または「指摘なし」: 必須修正finding 2件

  - `T004-NR-001`、severity `Medium`、origin `normal review`。場所: `src/NtpTimeSynchronizer.cpp:99`から`169`、公開境界は`include/NtpTimeSynchronizer.h:95`から`123`。説明: フォロワーTAGの`TryGetNodeSynchronization()`はローカル64bit時計からマスター時計の`synchronizedAtMasterTimeUs`を直接引き、`TryConvertLocalTimeToMaster()`は同期確定時の固定マスター時刻を32bit epoch拡張の参照に使う。時計domainが異なるため、同期経過時間は`nodeMinusMasterUs`分だけ誤り、同期後に固定参照から半epochを超えると時刻変換が別epochを選ぶ。影響: T-006以降でフォロワーのセンサー時刻または同期経過時間を公開すると、時刻が最大`2^32` usずれ、逐次結果の時系列が壊れる。再現: master時刻`5000`、follower時計offset `+200`のcommit直後にlocal full clockを`5201`として取得するとageは期待`1`に対して`201`となる。また同期master時刻を`(1ULL << 32) + 5000`とし、`(1ULL << 31) + 100` us後のlocal下位32bitを変換すると、固定参照に近い前epochが選ばれ、正解より`2^32` us小さい値となる。証拠: 設計6節は現在TAG時刻またはラウンド開始時刻に近いepochへの拡張、5.4節は同期経過時間提供、T-004は32bit折り返し変換を要求するが、既存testはcommit直後だけを検査する。必須修正条件: follower変換に呼出し時点の移動するマスター64bit参照を与えるか、ローカルfull clockとoffsetから同等の参照を安全に求め、ageも同じmaster domainで計算する。非0の正負offset、commit直後、32bit wrap、固定同期点から半epoch超のhost testを追加して正しい64bit値とageを検証する

  - `T004-NR-002`、severity `Medium`、origin `normal review`。場所: `src/NtpTimeSynchronizer.cpp:289`から`400`、`src/NtpTimeSynchronizer.cpp:85`から`97`。説明: `BuildTargets()`はmaster identity/sessionが変わった1回だけ呼ばれ、同一master session中に初めて有効になったANCHORまたはフォロワーTAGを追加する経路も公開APIもない。対象0件でも`IsSynchronizationComplete()`は直ちにtrueになる。影響: 初回NodeStatusの取りこぼし、起動時差、同一sessionへの後参加があると、そのノードは以後の1秒NodeStatusを受信しても同期要求を一度も受けず、T-007が同期済み測距経路を構築できない。再現: 有効self-master/sessionとremote 0件で`Update()`し、その後同じsessionの有効ANCHORまたはfollower TAGを`EspNowBroadcast`へ追加して`Update()`を繰り返しても、masterが同一なので`BuildTargets()`は再実行されず、送信要求件数は増えず同期情報も得られない。証拠: 設計5.4節は各ANCHOR・各follower TAGの同期、12.3節は有効ノード収集後の同期開始を要求する。実装レポートも同一session後参加を未解決リスクとして記録しており、現在の公開APIでは後続統合から解消できない。必須修正条件: 同一session中も有効かつ一意な未処理ノードを決定的に列挙して追加する、または後続controllerがsessionを壊さず対象一覧を更新・同期開始できる明示APIを提供する。既存完了対象の不要な再同期を避け、ANCHORとfollower TAG、ID昇順、重複ID除外、期限切れ、最大件数、初回0件後の追加をPlatformIO host testで検証する

## 結果

- 結果: verdict `fail`。review modeはinitial normal review、reviewer identityは`/root/t004_normal_review`で実装担当とは別、レビュー中に実装・test・trackingを変更していない。branchは`codex/multitag-sequential-ranging`、base/current HEADおよび`reviewed_implementation_head`は`50002aa6b32deb6a8f606da14c11a44ee6c968e1`、技術レビュー対象は同HEAD上の未コミットT-004差分、reviewed worktree fingerprintはSHA-256 `cdc4ab8b1edac2c78306aabb123e0eeb113c63737b0d14b0dbcf7a30ca64759f`。技術判定はHEADとfingerprintの組へだけ適用し、`report_attestation_allowed=false`、reserved report pathは`reports/T-004-ntp-time-synchronizer-normal-review.md`。NTPの`t1/t2/t3/t4`取得位置、offset/RTT式、符号付き64bit計算、100 ms、3回試行と最小RTT、session/sequence/target/source/destination/channel検証、ANCHORとfollower TAGの通信、master session reset、`SyncCommit`内容、品質優先順位、異種packet FIFO非破壊、送信idle、wire packingと24/27/34 byteおよび250 byte上限、`main.cpp`境界、日本語Doxygen、命名、PlatformIO Test Runner経路、scope規律、tracking/実装レポート整合、error validationは`checked_no_finding`。フォロワー時刻APIと同期経過時間、同一sessionノード列挙、後続T-006/T-007接続、対応test十分性は`checked_finding`。secret/securityは`not_applicable`。current-HEAD CIは`held`、実機検証は`held`。`native_t004` 9/9、`native` 5/5、M5StickS3 clean/full build、`git diff --check`は成功。full buildはRAM 52,136 / 327,680 bytes（15.9%）、Flash 1,216,735 / 3,342,336 bytes（36.4%）。unexploredなし。次actionは2 finding修正と対応host test追加後、同reviewerによるfix verification

## リスク

- 未解決のリスクまたは後続対応: heldは、実機ESP-NOWでの`rx_ctrl->timestamp`と`esp_timer_get_time()`の同一clock安定性、`rx_ctrl`欠落fallback、TAG 2台・ANCHOR 3台での3サンプル交換、packet loss、Wi-Fi省電力ON/OFF品質差、clock drift、実機master交代。これらは設計20.3節と`tasks-status.md`の実機保留項目に所有先があり、今回の2 findingとは区別する。matching current-HEAD CIは未コミットworking treeのため存在せずheld。repo-localの`tools/lint/`、`package.json`、`cspell.config.jsonc`が存在しないためMarkdown Word Checkerのfocused/full lintは`unsupported`であり、本文の手動用語確認、未解決placeholder不在、backtick/quoteによるlint回避なしを確認した。full buildのWindows long-path警告はbuild成功に影響しない。必須修正findingが残るためmerge、commit、stage、pushは行わない
