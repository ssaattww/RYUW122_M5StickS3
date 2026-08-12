# Preferencesコマンド仕様

ESP32の`Preferences`ライブラリを使用し、NT-ShellからNVS内の設定値を確認・変更する。
実装は`PreferenceCommands`クラスへまとめ、`main.cpp`にはコマンド処理を書かない。

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
- `pref list`は各設定を`ITEM <key> <type> <value>`で表示し、最後に件数と型情報エラー数を返す。
- 型情報がない、または矛盾するキーが1件でもある場合、`pref list`の最終行は`ERROR`から始まる。
