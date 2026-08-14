#pragma once

#include <Arduino.h>
#include <nvs.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

/**
 * @brief NVSアクセス結果を表します。
 */
enum class EnNvsResult
{
    Ok,
    NotStarted,
    InvalidNamespace,
    InvalidKey,
    NotFound,
    InvalidType,
    TypeMetadataNotFound,
    TypeMismatch,
    InvalidValue,
    InvalidBoolean,
    ReadFailed,
    SaveFailed,
    MetadataSaveFailed,
    RemoveFailed,
    ClearFailed,
    ListFailed,
};

/**
 * @brief NVSへ保存する値の論理型を表します。
 */
enum class EnNvsValueType : uint8_t
{
    Unknown = 0,
    Bool,
    I8,
    U8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,
    Float,
    Double,
    String,
};

/**
 * @brief NVS一覧の1項目を表します。
 */
struct NvsEntryInfo
{
    char m_key[16]{};
    EnNvsValueType m_type = EnNvsValueType::Unknown;
    EnNvsResult m_result = EnNvsResult::Ok;
};

/**
 * @brief 型メタデータ付きNVS値を管理します。
 * 値と型メタデータの異なる名前空間を同時に所有し、NT-Shellへ依存しません。
 * 公開操作は再帰ミューテックスで排他します。
 * 読み出し失敗時は明示的な結果を返し、出力引数を変更しません。
 */
class NvsPreferenceStore
{
public:
    /**
     * @brief 一覧項目を受け取るコールバック型です。
     *
     * @param entry 一覧項目
     * @param context 呼び出し元のコンテキスト
     */
    using EntryVisitor = void (*)(const NvsEntryInfo& entry, void* context);

    /**
     * @brief 使用する値名前空間と型情報名前空間を設定します。
     * 2つの名前空間は異なる名前を指定します。
     *
     * @param namespaceName 値を保存するNVS名前空間
     * @param metadataNamespaceName 型情報を保存するNVS名前空間
     */
    NvsPreferenceStore(
        const char* namespaceName,
        const char* metadataNamespaceName);

    /**
     * @brief 開いているNVS名前空間を閉じます。
     */
    ~NvsPreferenceStore();

    /**
     * @brief NVS所有権の重複を防ぐためコピー構築を禁止します。
     *
     * @param other コピー元
     */
    NvsPreferenceStore(const NvsPreferenceStore& other) = delete;

    /**
     * @brief NVS所有権の重複を防ぐためコピー代入を禁止します。
     *
     * @param other コピー元
     * @return このインスタンス
     */
    NvsPreferenceStore& operator=(const NvsPreferenceStore& other) = delete;

    /**
     * @brief 値と型情報のNVS名前空間を読み書き可能な状態で開きます。
     * 同名の名前空間は値の上書きを防ぐため拒否します。
     *
     * @return 開始結果
     */
    EnNvsResult Begin();

    /**
     * @brief 開いているNVS名前空間を閉じます。
     */
    void End();

    /**
     * @brief NVS名前空間が利用可能か確認します。
     *
     * @return 利用可能な場合はtrue、それ以外はfalse
     */
    bool IsStarted() const;

    /**
     * @brief 値保存用のNVS名前空間名を取得します。
     *
     * @return 値保存用名前空間名
     */
    const char* GetNamespaceName() const;

    /**
     * @brief 型情報保存用のNVS名前空間名を取得します。
     *
     * @return 型情報保存用名前空間名
     */
    const char* GetMetadataNamespaceName() const;

    /**
     * @brief NVSキーを検証します。
     *
     * @param key 検証するキー
     * @return 検証結果
     */
    EnNvsResult ValidateKey(const char* key) const;

    /**
     * @brief 指定キーが値名前空間に存在するか確認します。
     *
     * @param key 確認するキー
     * @param exists 存在結果の格納先
     * @return 処理結果
     */
    EnNvsResult Exists(const char* key, bool& exists) const;

    /**
     * @brief NVSパーティションの未使用エントリ数を取得します。
     * Arduino `Preferences::freeEntries()`と同じ`free_entries`を返します。
     *
     * @param count 空きエントリ数の格納先
     * @return 処理結果
     */
    EnNvsResult GetFreeEntries(size_t& count) const;

    /**
     * @brief 指定キーの論理型を取得します。
     *
     * @param key 取得するキー
     * @param type 論理型の格納先
     * @return 処理結果
     */
    EnNvsResult GetValueType(const char* key, EnNvsValueType& type) const;

    /**
     * @brief 論理型の表示名を取得します。
     *
     * @param type 論理型
     * @return 表示名
     */
    static const char* GetValueTypeName(EnNvsValueType type);

    /**
     * @brief 論理型の名前を解析します。
     *
     * @param typeName 解析する型名
     * @param type 解析結果の格納先
     * @return 処理結果
     */
    static EnNvsResult ParseValueType(
        const char* typeName,
        EnNvsValueType& type);

    /**
     * @brief bool値を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetBool(const char* key, bool& value) const;

    /**
     * @brief 8bit符号付き整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetI8(const char* key, int8_t& value) const;

    /**
     * @brief 8bit符号なし整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetU8(const char* key, uint8_t& value) const;

    /**
     * @brief 16bit符号付き整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetI16(const char* key, int16_t& value) const;

    /**
     * @brief 16bit符号なし整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetU16(const char* key, uint16_t& value) const;

    /**
     * @brief 32bit符号付き整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetI32(const char* key, int32_t& value) const;

    /**
     * @brief 32bit符号なし整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetU32(const char* key, uint32_t& value) const;

    /**
     * @brief 64bit符号付き整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetI64(const char* key, int64_t& value) const;

    /**
     * @brief 64bit符号なし整数を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetU64(const char* key, uint64_t& value) const;

    /**
     * @brief float値を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetFloat(const char* key, float& value) const;

    /**
     * @brief double値を取得します。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetDouble(const char* key, double& value) const;

    /**
     * @brief 文字列を取得します。
     * NVS読み出しまたは一時領域確保が失敗した場合は出力値を変更しません。
     *
     * @param key キー
     * @param value 値の格納先
     * @return 処理結果
     */
    EnNvsResult GetString(const char* key, String& value) const;

    /**
     * @brief bool値を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetBool(const char* key, bool value);

    /**
     * @brief 8bit符号付き整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetI8(const char* key, int8_t value);

    /**
     * @brief 8bit符号なし整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetU8(const char* key, uint8_t value);

    /**
     * @brief 16bit符号付き整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetI16(const char* key, int16_t value);

    /**
     * @brief 16bit符号なし整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetU16(const char* key, uint16_t value);

    /**
     * @brief 32bit符号付き整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetI32(const char* key, int32_t value);

    /**
     * @brief 32bit符号なし整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetU32(const char* key, uint32_t value);

    /**
     * @brief 64bit符号付き整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetI64(const char* key, int64_t value);

    /**
     * @brief 64bit符号なし整数を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetU64(const char* key, uint64_t value);

    /**
     * @brief float値を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetFloat(const char* key, float value);

    /**
     * @brief double値を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetDouble(const char* key, double value);

    /**
     * @brief 空文字列を含む文字列を保存します。
     *
     * @param key キー
     * @param value 保存値
     * @return 処理結果
     */
    EnNvsResult SetString(const char* key, const String& value);

    /**
     * @brief 指定キーの値と型情報を削除します。
     *
     * @param key 削除するキー
     * @return 処理結果
     */
    EnNvsResult Remove(const char* key);

    /**
     * @brief 値と型情報の名前空間を消去します。
     *
     * @return 処理結果
     */
    EnNvsResult Clear();

    /**
     * @brief 全項目を列挙し、型情報の整合性も通知します。
     * コールバックは再帰ロック中に同期実行され、同じ実行コンテキストからの読み取り再入は許可します。
     * 列挙中のイテレータを保護するため、コールバックから同ストアを更新してはいけません。
     * コールバックから別タスクの同ストア操作完了を待ってはいけません。
     *
     * @param visitor 項目ごとに呼び出すコールバック
     * @param context コールバックへ渡すコンテキスト
     * @param itemCount 成功時に正常項目数を格納する出力先
     * @param errorCount 成功時に型情報エラー項目数を格納する出力先
     * @return 列挙処理結果
     */
    EnNvsResult List(
        EntryVisitor visitor,
        void* context,
        size_t& itemCount,
        size_t& errorCount) const;

private:
    /**
     * @brief 指定型で値を読み出せるか検証します。
     *
     * @param key 検証するキー
     * @param expectedType 期待する論理型
     * @return 検証結果
     */
    EnNvsResult ValidateRead(
        const char* key,
        EnNvsValueType expectedType) const;

    /**
     * @brief 保存済み値と型情報の整合性を検証します。
     *
     * @param key 検証するキー
     * @param type 論理型
     * @return 検証結果
     */
    EnNvsResult ValidateStoredValue(
        const char* key,
        EnNvsValueType type) const;

    /**
     * @brief 型メタデータを取得します。
     *
     * @param key 取得するキー
     * @param type 論理型の格納先
     * @return 処理結果
     */
    EnNvsResult GetMetadataType(
        const char* key,
        EnNvsValueType& type) const;

    /**
     * @brief 値保存後に型メタデータを保存します。
     *
     * @param key 保存するキー
     * @param type 論理型
     * @param valueSetResult 値の書き込み結果
     * @return 処理結果
     */
    EnNvsResult FinishSet(
        const char* key,
        EnNvsValueType type,
        esp_err_t valueSetResult);

    /**
     * @brief 未コミット変更を破棄するためNVSハンドルを開き直します。
     *
     * @return 再開始結果
     */
    EnNvsResult ReopenAfterWriteFailure();

    /**
     * @brief 読み出しAPIのESP-IDF結果を共通結果へ変換します。
     *
     * @param result ESP-IDFのNVS処理結果
     * @return 共通結果
     */
    static EnNvsResult ConvertReadResult(esp_err_t result);

    mutable std::recursive_mutex m_mutex;
    nvs_handle_t m_valueHandle = 0;
    nvs_handle_t m_metadataHandle = 0;
    std::string m_namespace;
    std::string m_metadataNamespace;
    bool m_started = false;
};
