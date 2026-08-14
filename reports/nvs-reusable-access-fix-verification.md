# Sub-agent実行レポート

## タスク

- 目的: NVS再利用APIの通常レビュー指摘NR-001〜NR-004と規約指摘S-001/S-002の修正を検証する。
- タスク種別: fix verification

## sub-agentを使う理由

- 理由: finding identityとseverityを維持し、同一通常レビュアーが修正を追跡するため。

## 対象範囲

- 対象: 修正差分、各findingの直接原因、同種ケース、新規回帰、ビルド証跡。

## 対象外

- 対象外: 実装修正、Git操作、実機試験、ESP-NOW/UI変更。

## 実行コマンド

- 実行コマンド: `work-context-manager`、`review-worker`、`report-writer`と本予約reportの全文確認、`Get-Content`による通常レビュー・規約レビュー・fix実装report・修正後source/header/docsの行番号付き確認、`rg`による全公開操作のlock、status付きNVS read、List再入、error propagation、sibling call site、依存API契約のscan、導入済みESP-IDF 5 `nvs.h`とArduino ESP32 3.3.8 `WString.h`/`WString.cpp`/`Preferences.cpp`の直接確認、`.git/HEAD`と`.git/refs/heads/master`の直接読取、`Get-FileHash -Algorithm SHA256`による対象同一性確認、既存object/firmware artifactと実装担当clean/full build reportの照合。指示どおりbuild再実行、Gitコマンド、実装修正、nested agent実行は行っていない。

## 対象ファイル

- 変更または確認したファイル: 修正対象の`include/NvsPreferenceStore.h`、`src/NvsPreferenceStore.cpp`、`src/PreferenceCommands.cpp`、`docs/preferences-commands.md`、`reports/nvs-reusable-access-fixes.md`、追跡元の`reports/nvs-reusable-access-normal-review.md`、`reports/nvs-reusable-access-standards-verification.md`を確認した。sibling/new-impact確認として`include/PreferenceCommands.h`、`include/ConfigPreference.h`、`src/ConfigPreference.cpp`、`src/main.cpp`のNVS composition、`platformio.ini`、Arduino `Preferences.cpp`と`WString.h`/`WString.cpp`、IDF `nvs.h`、既存build artifactを確認した。変更したファイルは本reportのみ。

## 指摘事項

- 指摘要約または「指摘なし」: 未解消1件（source severity mediumを維持）、新規1件（low）。severity reclassificationはない。
  - `NVS-REUSE-NR-002`（medium、通常レビュー起点、未解消）: `src/NvsPreferenceStore.cpp:408-439`、`src/PreferenceCommands.cpp:491-567`。数値11型はstatus付きIDF read、一時変数、成功時out代入へ修正された。一方`GetString`は検証済み`storedValue`を最後に`value = storedValue`でコピーし、直後に無条件で`Ok`を返す。依存実装`WString.cpp:235-243,276-280`ではコピー先確保失敗時にout Stringを`invalidate()`するがoperator=から成否を取得できないため、メモリ不足で「失敗時out非変更」「明示ReadFailed」が再び破られる。さらにsibling callerの`WriteListValue`は全12型の`Get*`戻り値を無視するため、Listの型検査後に実readが失敗すると既定値の`ITEM`を出力し、最終行も`OK`になり得る。影響は設定値の破壊的out更新と、CLI一覧の誤成功・誤値表示。必要対応は、検証済み一時Stringをmoveして追加確保なしでoutへcommitする等の失敗不能な最終代入にし、`WriteListValue`/`ListContext`でgetter結果を収集して`ReadFailed`を最終結果へ反映し、allocation/read失敗のsibling testを追加すること。
  - `NVS-REUSE-FV-001`（low、fix verification新規）: `src/NvsPreferenceStore.cpp:139-154`。旧実装の`Preferences::freeEntries()`は依存実装`Preferences.cpp:562-570`で`nvs_stats_t::free_entries`を返していたが、直接NVS化後は`available_entries`を返す。公開API名`GetFreeEntries`と既存`pref status`の`free_entries=`出力を維持したまま数値の意味だけが変わり、既存コマンド/API互換性を損なう。必要対応は`stats.free_entries`へ戻すか、意図的な意味変更ならAPI・出力名・設計を明示的に変更して承認を得ること。

## 結果

- 結果: verdictは`fail`。review modeはfix verification、reviewerは初回通常レビューと同じ`nvs_reusable_review`で、実装担当Newtonの修正には関与していない。finding dispositionは`S-001` High=`resolved`（同名namespaceを`InvalidNamespace`）、`NVS-REUSE-NR-001` Medium=`resolved`（同一storeの全handle操作をrecursive mutexで排他し、型検証から実readを同一lock、Begin/End raceも排除。List visitorの同一実行コンテキスト読取再入は成立し、更新再入・別task待機禁止をheader/docsで明示）、`NVS-REUSE-NR-002` Medium=`open`（上記残存）、`NVS-REUSE-NR-003` Medium=`resolved`（空文字列を`nvs_set_str`のstatusで成功判定しmetadataもcommit）、`NVS-REUSE-NR-004` Low=`resolved`、`S-002` Medium=`resolved`（いずれもList局所countを完全成功時だけoutへcommit）。source severityは全件維持し、reclassification/erratumなし。対象identityはbranch `master`、base/current/reviewed HEAD `05b4575bd03aa6c4bebfdcb5caa5727df5f11e83` + current working tree。主要対象SHA-256は`docs/preferences-commands.md=5759CCA1249CD2FD08F9826073A8DD77104CDB4F270979F5CCB7CFBFA336FEA9`、`include/PreferenceCommands.h=3ED5C23F9CE42D361196D47C6819E2FB9ED329D9F2DAE586B193ACE37BF8E23F`、`src/PreferenceCommands.cpp=6CC7AEE914C93CECAABB296E4058EE0B1C5C09D55B65FAF9F647A55C30CA2820`、`include/ConfigPreference.h=C21C19FBCEF798E3C3AFB8B783D300E576A484E64BE8004D0FFDE6D80054A221`、`src/ConfigPreference.cpp=463FDC47FF36532E99E0D6AF3CFAAB406F54CB24E13EED41BCE185CF75FC6A88`、`include/NvsPreferenceStore.h=A5C1B464113CC72587806977476FFD41AEAD4D7FD029BF0156878C2BFC358781`、`src/NvsPreferenceStore.cpp=E9C9D71F8DC7F1B5873A87A5AF295B5DA97E6792D171C5B92139BA21E841372A`、`src/main.cpp=385639FAA0A945B3541FA7B2A9D717E20642B6F056F396CB1EA8546A8CA4C913`、`reports/nvs-reusable-access-fixes.md=5F8A934E4108D4030C2D2DBCB5690634C7A57966A21F6000E78B5933D604CE99`。coverage dispositionは要件/design=`checked_finding`、correctness/edge cases=`checked_finding`、scope discipline=`checked_no_finding`、changed files/direct deps=`checked_finding`、API/data/config compatibility=`checked_finding`、error handling=`checked_finding`、security/secrets=`checked_no_finding`、tests/validation=`checked_finding`、docs/tracking=`checked_finding`、CI=`not_applicable`、regression/maintainability=`checked_finding`。実装担当のclean/full build SUCCESS、対象4 translation unit再compile、RAM 15.3%、Flash 35.5%とartifactを確認したが、実機・自動testはなく上記2件を検出していない。また最終header/docsのmtimeはobject生成後で、current target exact-byteの再build証跡ではない。次actionはopen/new findingを修正し、failure-focused testとcurrent exact targetのclean/full build後に再fix verificationを行うこと。reserved pathは`reports/nvs-reusable-access-fix-verification.md`、未コミット対象のため`report_attestation_allowed=false`、mergeは許可しない。

## リスク

- 未解決のリスクまたは後続対応: heldは実機NVSの全12型・空文字列・再起動後読出し、同時taskアクセス、メモリ/flash障害注入、別namespace間の非transaction性、値commit後のmetadata commit失敗、visitor更新再入禁止が契約依存でruntime強制されない点、現在header/docsがbuild artifactより後に更新された検証時系列、Windows Long Path警告。Markdown wording checkは対象rootと本reportを確定したが、`tools/lint/`、`package.json`、`cspell.config.jsonc`がなくfocused/fullとも`unsupported`でpass扱いしていない。backtickはfinding ID、API、型、ファイル、コマンド等の正当な用途で、通常文のlint回避利用はない。unexploredは指示により対象外のESP-NOW/UI既存ユーザー差分と実機動作。自動testは0件、matching CI/workflowもない。対象が後続で変更された場合は本verdictを引き継がない。
