# Sub-agent実行レポート

## タスク

- 目的: NT-Shell非依存NVS再利用APIの通常レビューを行う。
- タスク種別: 通常コードレビュー

## sub-agentを使う理由

- 理由: review-enforcerが実装担当と異なるsub-agentによるレビューを必須としているため。

## 対象範囲

- 対象: NvsPreferenceStore、ConfigPreference、PreferenceCommands、mainのcomposition、関連設計、直接依存と検証証跡。

## 対象外

- 対象外: 実装修正、Git操作、実機試験、ESP-NOW/UIの既存ユーザー変更。

## 実行コマンド

- 実行コマンド: 指定された4 Skillと本予約reportの全文確認、`Get-Content`による対象全ファイル・直接依存の行番号付き確認、`rg`による参照・依存分離・fallback・命名・テスト・lint設定のscan、導入済みArduino ESP32 3.3.8の`Preferences.h`/`Preferences.cpp`とIDF 5の`nvs.h`契約確認、`.git/HEAD`と`.git/refs/heads/master`の直接読取、`Get-FileHash -Algorithm SHA256`による開始時・終了前の対象同一性確認、既存build artifactと実装reportの証跡確認、`markdown-word-checker`に基づくfocused/full lint wiringとbacktick用途の確認。指示どおりbuild再実行、Gitコマンド、実装修正、nested agent実行は行っていない。

## 対象ファイル

- 変更または確認したファイル: レビュー対象の`docs/preferences-commands.md`、`include/PreferenceCommands.h`、`src/PreferenceCommands.cpp`、`include/ConfigPreference.h`、`src/ConfigPreference.cpp`、`include/NvsPreferenceStore.h`、`src/NvsPreferenceStore.cpp`、`src/main.cpp`のNVS composition（21-23、125-126行）、`reports/nvs-reusable-access-implementation.md`を確認した。直接依存として`platformio.ini`、`include/NtShell.h`、`src/NtShell.cpp`、Arduino ESP32 3.3.8の`libraries/Preferences/src/Preferences.h`/`Preferences.cpp`、IDF 5の`nvs.h`、`test/README`、既存build artifactを確認した。変更したファイルは本reportのみ。

## 指摘事項

- 指摘要約または「指摘なし」: 必須指摘4件（medium 3件、low 1件）。
  - `NVS-REUSE-NR-001`（medium、通常レビュー起点）: `src/NvsPreferenceStore.cpp:122-156,207-399,426-471`。共有storeの値・metadataをまたぐ複合操作に排他がなく、NT-Shellスレッドとdomain側が同じキーへ同時アクセスすると、型検証後かつ実値取得前の型変更、または値保存後かつmetadata保存前の割込みが可能である。たとえば`GetU8`が`PT_U8`/`U8`を検証した直後に`SetString`が完了すると、後続`getUChar`は既定値0を返し得るのに`GetU8`は`Ok`を返し、`ConfigPreference`が誤って`Tag`を採用する。共有store注入という主要要件で、型付きAPIが誤値を成功として返す影響がある。公開操作の検証・実値アクセス・metadata更新を同一の同期境界に入れ、`List` callbackからの再入を含むlock設計を明示し、競合テストを追加すること。
  - `NVS-REUSE-NR-002`（medium、通常レビュー起点）: `src/NvsPreferenceStore.cpp:207-300`。全typed Getは`ValidateRead`成功後に、エラー状態を返さず既定値を返すArduino `Preferences::get*`を再度呼び、その結果をout parameterへ代入して無条件に`Ok`を返す。依存実装`Preferences.cpp:356-516`では読出しエラー時に0/false/空文字列/NAN等へfallbackするため、競合がなくても2回目のflash read失敗や`String`確保失敗を明示できず、「失敗時out非変更」「不要fallbackなし」「明示EnNvsResult」に反する。値は一時変数へstatus付きIDF API等で1回だけ読み、成功時だけoutへcommitし、読出し失敗を表す結果を返すこと。全12型の失敗注入テストを追加すること。
  - `NVS-REUSE-NR-003`（medium、通常レビュー起点）: `src/NvsPreferenceStore.cpp:393-398,565-574`。Arduino ESP32 3.3.8の`Preferences::putString`は空文字列をNVSへ正常保存・commitした後でも`strlen(value)`、すなわち0を返すが、`FinishSet`は0を`SaveFailed`と判定してmetadataを書かない。このため`SetString(key, String())`は失敗を返しながら値だけを変更し、新規キーでは必須metadata欠落、既存異型キーでは型不整合を作る。空文字列を有効なstringとして成功判定できる保存経路に変更し、空文字列の新規保存・上書き・再読出し・一覧・削除を検証すること。
  - `NVS-REUSE-NR-004`（low、通常レビュー起点）: `src/NvsPreferenceStore.cpp:426-471`。`List`は`itemCount`と`errorCount`を先に0へ更新してから`NotStarted`/`InvalidValue`を返し、走査途中の`ListFailed`でも部分件数を残すため、文書化された「out parameterは成功時だけ更新」に反する。局所countで集計し、正常終了時だけ呼出し元outへ代入すること。未開始、null visitor、iterator異常の各失敗テストを追加すること。

## 結果

- 結果: verdictは`fail`。review modeはinitial normal review、reviewerは実装担当Newtonと別の`nvs_reusable_review`であり実装・修正には関与していない。対象identityはbranch `master`、base/current/reviewed HEAD `05b4575bd03aa6c4bebfdcb5caa5727df5f11e83`に未コミットworking-tree対象を重ねたもの。開始時と終了前でHEAD参照と対象SHA-256は一致し、unstableではない。対象hashは`docs/preferences-commands.md=F644E6EDA872A1A5AA82CE4C49C35E96F6153257BBEE81AFED36E501E8CE6A5C`、`include/PreferenceCommands.h=3ED5C23F9CE42D361196D47C6819E2FB9ED329D9F2DAE586B193ACE37BF8E23F`、`src/PreferenceCommands.cpp=5855F103F3CF86D486025A8E7535F99825B2191879CE498845E07B0E17DA4908`、`include/ConfigPreference.h=C21C19FBCEF798E3C3AFB8B783D300E576A484E64BE8004D0FFDE6D80054A221`、`src/ConfigPreference.cpp=463FDC47FF36532E99E0D6AF3CFAAB406F54CB24E13EED41BCE185CF75FC6A88`、`include/NvsPreferenceStore.h=0C8205DCF0B6E11F80942533CE76D2ED9B3923FAABA171AA5679BAA9F24D47B5`、`src/NvsPreferenceStore.cpp=AAC00921CF245D4393A4F0B3D895D39F6C6E0A984564CFC099DC1EC988F28545`、`src/main.cpp=385639FAA0A945B3541FA7B2A9D717E20642B6F056F396CB1EA8546A8CA4C913`、`reports/nvs-reusable-access-implementation.md=16906F573743F50A1B29DCE7239ADE021E53DA58292FE5653CC49C038A31B2EF`。coverage dispositionは、要件・設計=`checked_finding`、correctness/edge cases=`checked_finding`、scope discipline=`checked_no_finding`、全changed files/direct deps=`checked_finding`、API/data/config compatibility=`checked_finding`（既存`ryuw122`/`ryuw122_meta`と1-byte型IDは維持）、error handling=`checked_finding`、security/secrets=`checked_no_finding`、tests/validation=`checked_finding`、docs/tracking=`checked_finding`、CI=`not_applicable`（workflowなし）、regression/maintainability=`checked_finding`。提供済みclean/full build SUCCESS、対象全compile、RAM 15.3%、Flash 35.6%の証跡と生成artifactは確認したが、buildのみでは上記動作欠陥を検出できない。次actionは4件を修正し、focused testとclean/full build後にfix verificationを行うこと。reserved report pathは`reports/nvs-reusable-access-normal-review.md`、未コミット実装対象のため`report_attestation_allowed=false`、mergeは許可しない。

## リスク

- 未解決のリスクまたは後続対応: heldは実機NVSでの全12型保存・再起動後読出し・破損metadata・flash障害、別namespace間の非transaction性、metadataだけが残った状態を`Remove`が`NotFound`として清掃しない点、Windows Long Path警告。Markdown wording checkは対象rootと本reportを確定したが、`tools/lint/`、`package.json`、`cspell.config.jsonc`がすべてなくfocused/fullとも`unsupported`で、pass扱いしていない。backtickは識別子・API・ファイル・コマンド等の正当な用途であり、普通文をlint回避のために囲った箇所はない。unexploredは指示により対象外とした`src/main.cpp`のESP-NOW/UI既存ユーザー差分と実機動作で、今回の技術verdictへは含めない。自動テストは`test/README`のみで実テスト0件、matching CIも存在しない。レポートは未コミット対象に対する通常レビュー証跡であり、後続の対象変更にはverdictを引き継がない。
