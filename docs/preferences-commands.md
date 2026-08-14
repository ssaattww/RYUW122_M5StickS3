# Preferencesコマンド仕様

ESP32の`Preferences`ライブラリを使用し、NVS内の設定値を確認・変更する。
NVSアクセスは`NvsPreferenceStore`が所有し、`PreferenceCommands`はNT-Shellのコマンド解析と表示だけを担当する。
`main.cpp`にはコマンド処理を書かない。

参考: [Preferencesライブラリの使い方](https://progkeiyou.com/preferences/)

## 名前空間

- 既定のNVS名前空間は`ryuw122`とする。
- 型情報のNVS名前空間は`ryuw122_meta`とする。
- 名前空間とキーはESP32 NVSの制約に合わせて15文字以内とする。
- コマンドはNT-Shell専用スレッドで逐次処理する。

## コマンド

```text
pref status
pref list
pref exists <key>
pref get <type> <key>
pref set <type> <key> <value>
pref remove <key>
pref clear YES
```

`clear`は誤操作を防ぐため、確認文字列`YES`を必須とする。
`list`は現在の名前空間に保存されている全キーを、キー名・保存時に指定した型・値の順で表示する。
`set`で値を保存するとき、同じキーの型IDを型情報名前空間へ保存する。
`remove`と`clear`は値と型情報の両方を削除する。
型情報は必須とする。型情報がないキーや、型情報と値のNVS型が矛盾するキーはエラーとして表示する。

対応する`type`は次のとおり。

```text
bool i8 u8 i16 u16 i32 u32 i64 u64 float double string
```

`string`の設定値は空白を含められる。コマンド行の残りの引数を空白で連結して保存する。
NT-Shellの制約により、コマンド行は終端文字を含めて64文字以内とする。

読み出し時は保存時の型情報を必ず検査し、型情報がない場合や指定型と一致しない場合はエラーを返す。
Preferencesの`bool`値は0を`false`、1を`true`とし、それ以外は`ERROR invalid_boolean`を返す。

## 出力

- 成功時は`OK`から始まる1行を返す。
- 失敗時は`ERROR`から始まる1行を返す。
- `pref get`は`OK <key> <type> <value>`の形式で返す。
- `pref list`は読み出しに成功した設定だけを`ITEM <key> <type> <value>`で表示する。
- 型情報がない、型が矛盾する、または実値読み出しが失敗した項目は`ERROR <key> <error>`で表示する。初期値を`ITEM`として表示しない。
- `pref list`の最終行は`OK|ERROR count=<ITEM行数> metadata_errors=<事前検証エラー数> read_errors=<最終実値読み出しエラー数>`とする。`metadata_errors`は`List`が行うmetadata・保存型・必要サイズの事前検証失敗を含む。どちらかのエラー数が1以上、または列挙件数の整合性が崩れた場合は`ERROR`から始める。
- `pref status`の`free_entries`はArduino `Preferences::freeEntries()`と同じNVS statisticsの`free_entries`を表す。

## 再利用API

`NvsPreferenceStore`はNT-Shellと`Stream`に依存せず、値名前空間と型情報名前空間の開始・終了を一括して所有する。
値と型情報には必ず異なるNVS名前空間を指定し、同名の場合は`Begin`が`InvalidNamespace`を返す。
アプリケーションは1個のストアを生成して`Begin`を1回呼び出し、コマンド層とdomain固有設定層へ参照を渡す。
ストアの破棄時には開いている名前空間が自動的に閉じられ、明示的に閉じる場合は`End`を使用する。

同じストアへの`Begin`、`End`、取得、保存、存在確認、削除、全削除、列挙は再帰ミューテックスで相互排他される。
`List`のvisitorは排他区間内で同期呼び出しされ、同じ実行コンテキストからストアの読み取りAPIへ再入できる。
列挙中のNVSイテレータを保護するため、visitorから同ストアの`Begin`、`End`、`Set`、`Remove`、`Clear`を呼び出してはいけない。
visitorは別タスクへ同ストアの操作を依頼し、その完了を待ってはいけない。

公開APIは次の責務を持つ。

- `EnNvsResult`は未開始、キー不正、未登録、型情報欠落、型不一致、読み出し失敗、保存失敗などを呼び出し元へ返す。
- `GetBool`から`GetString`と`SetBool`から`SetString`は、対応する12型を型情報と合わせて読み書きする。
- `Exists`、`Remove`、`Clear`は値名前空間と型情報名前空間の整合を維持する。
- `List`は各キーの`NvsEntryInfo`をコールバックへ渡し、metadataと保存型が整合する項目数と型情報エラー数を返す。コールバック内の実値読み出し結果はコールバック側が別に集計する。
- `ValidateKey`、`GetValueType`、`ParseValueType`はNT-Shell以外の呼び出し元でも同じ検証規則を利用できる。

out parameterを持つ取得・検証APIは成功時だけ出力値を更新し、失敗時は呼び出し元の値を変更しない。
NVSの型メタデータ検証からESP-IDF NVS APIによる実値読み出しまでは、同じ排他区間で実行する。
NVS APIの実読み出しが失敗した場合は`ReadFailed`を返し、暗黙のデフォルト値を成功値として返さない。
`GetString`はNVS読み出しバッファと一時`String`の確保結果を検査し、最終値を追加確保のないmove代入でout parameterへ反映する。

`SetString`は空文字列も正式な値として保存する。
値と型情報は別名前空間であり、ESP-IDF NVSは複数ハンドルを一括するtransactionを提供しない。
ストアはコミット前の失敗時にハンドルを開き直して未コミット変更を破棄し、値コミット後の型情報コミット失敗は`MetadataSaveFailed`として明示する。
電源断や後半のコミット失敗で部分更新になる可能性は残るが、次回読み出しで型情報欠落または型不一致として検出する。

構成例を次に示す。

```cpp
NvsPreferenceStore store("ryuw122", "ryuw122_meta");
PreferenceCommands commands(store);
ConfigPreference config(store);

store.Begin();
EnRunMode mode;
const EnNvsResult result = config.GetCurrentRunMode(mode);
```

`ConfigPreference`は同じストアの`run_mode`キーを`u8`型として使用する。
保存値は`EnRunMode::Tag`または`EnRunMode::Anchor`だけを許可し、欠落・型不一致・範囲外では結果コードを返して出力値を変更しない。
