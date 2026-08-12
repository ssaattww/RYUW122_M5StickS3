#include "PreferenceCommands.h"

#include <Arduino.h>

#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "nvs.h"

namespace
{
    constexpr size_t MaxNvsNameLength = 15;

    enum class EnStoredType : uint8_t
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
     * @brief コマンドで指定された型名を保存用の型IDへ変換します。
     *
     * @param typeName 変換する型名
     * @return 対応する型ID、未対応の場合はUnknown
     */
    EnStoredType ParseStoredType(const char* typeName)
    {
        if (strcmp(typeName, "bool") == 0) return EnStoredType::Bool;
        if (strcmp(typeName, "i8") == 0) return EnStoredType::I8;
        if (strcmp(typeName, "u8") == 0) return EnStoredType::U8;
        if (strcmp(typeName, "i16") == 0) return EnStoredType::I16;
        if (strcmp(typeName, "u16") == 0) return EnStoredType::U16;
        if (strcmp(typeName, "i32") == 0) return EnStoredType::I32;
        if (strcmp(typeName, "u32") == 0) return EnStoredType::U32;
        if (strcmp(typeName, "i64") == 0) return EnStoredType::I64;
        if (strcmp(typeName, "u64") == 0) return EnStoredType::U64;
        if (strcmp(typeName, "float") == 0) return EnStoredType::Float;
        if (strcmp(typeName, "double") == 0) return EnStoredType::Double;
        if (strcmp(typeName, "string") == 0) return EnStoredType::String;
        return EnStoredType::Unknown;
    }

    /**
     * @brief 保存用の型IDに対応する表示名を取得します。
     *
     * @param type 保存用の型ID
     * @return 型の表示名
     */
    const char* GetStoredTypeName(EnStoredType type)
    {
        switch (type)
        {
        case EnStoredType::Bool: return "bool";
        case EnStoredType::I8: return "i8";
        case EnStoredType::U8: return "u8";
        case EnStoredType::I16: return "i16";
        case EnStoredType::U16: return "u16";
        case EnStoredType::I32: return "i32";
        case EnStoredType::U32: return "u32";
        case EnStoredType::I64: return "i64";
        case EnStoredType::U64: return "u64";
        case EnStoredType::Float: return "float";
        case EnStoredType::Double: return "double";
        case EnStoredType::String: return "string";
        default: return "unknown";
        }
    }

    /**
     * @brief キーに保存された型IDを読み出します。
     *
     * @param preferences 型情報用Preferences
     * @param key 読み出すキー
     * @return 保存された型ID、存在しないか不正な場合はUnknown
     */
    EnStoredType GetStoredType(Preferences& preferences, const char* key)
    {
        if (!preferences.isKey(key))
        {
            return EnStoredType::Unknown;
        }

        const uint8_t value = preferences.getUChar(key);
        if (value < static_cast<uint8_t>(EnStoredType::Bool) ||
            value > static_cast<uint8_t>(EnStoredType::String))
        {
            return EnStoredType::Unknown;
        }

        return static_cast<EnStoredType>(value);
    }

    /**
     * @brief 符号付き整数の文字列を64bit整数へ変換します。
     *
     * @param text 変換する文字列
     * @param value 変換結果の格納先
     * @return 文字列全体を変換できた場合はtrue、それ以外はfalse
     */
    bool ParseSigned(const char* text, int64_t& value)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return false;
        }

        errno = 0;
        char* endPointer = nullptr;
        const long long parsedValue = strtoll(text, &endPointer, 0);
        if (errno == ERANGE || endPointer == text || *endPointer != '\0')
        {
            return false;
        }

        value = static_cast<int64_t>(parsedValue);
        return true;
    }

    /**
     * @brief 符号なし整数の文字列を64bit整数へ変換します。
     *
     * @param text 変換する文字列
     * @param value 変換結果の格納先
     * @return 文字列全体を変換できた場合はtrue、それ以外はfalse
     */
    bool ParseUnsigned(const char* text, uint64_t& value)
    {
        if (text == nullptr || text[0] == '\0' || text[0] == '-')
        {
            return false;
        }

        errno = 0;
        char* endPointer = nullptr;
        const unsigned long long parsedValue = strtoull(text, &endPointer, 0);
        if (errno == ERANGE || endPointer == text || *endPointer != '\0')
        {
            return false;
        }

        value = static_cast<uint64_t>(parsedValue);
        return true;
    }

    /**
     * @brief 浮動小数点数の文字列をdoubleへ変換します。
     *
     * @param text 変換する文字列
     * @param value 変換結果の格納先
     * @return 有限値へ変換できた場合はtrue、それ以外はfalse
     */
    bool ParseFloatingPoint(const char* text, double& value)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return false;
        }

        errno = 0;
        char* endPointer = nullptr;
        const double parsedValue = strtod(text, &endPointer);
        if (errno == ERANGE ||
            endPointer == text ||
            *endPointer != '\0' ||
            !std::isfinite(parsedValue))
        {
            return false;
        }

        value = parsedValue;
        return true;
    }

    /**
     * @brief 64bit符号付き整数を文字列の末尾へ追加します。
     *
     * @param output 追加先の文字列
     * @param value 追加する値
     */
    void AppendSigned(String& output, int64_t value)
    {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
        output += buffer;
    }

    /**
     * @brief 64bit符号なし整数を文字列の末尾へ追加します。
     *
     * @param output 追加先の文字列
     * @param value 追加する値
     */
    void AppendUnsigned(String& output, uint64_t value)
    {
        char buffer[32];
        snprintf(
            buffer,
            sizeof(buffer),
            "%llu",
            static_cast<unsigned long long>(value));
        output += buffer;
    }

    /**
     * @brief 改行を含む1行を1回のストリーム書き込みで送信します。
     *
     * @param output 出力先
     * @param text 改行を含まない出力文字列
     * @return 送信できた場合はtrue、文字列領域を確保できない場合はfalse
     */
    bool WriteLine(Stream& output, String text)
    {
        if (!text.reserve(text.length() + 2))
        {
            return false;
        }

        text += "\r\n";
        return output.write(
            reinterpret_cast<const uint8_t*>(text.c_str()),
            text.length()) == text.length();
    }

}

/**
 * @brief 指定されたNVS名前空間を使用するコマンド群を生成します。
 *
 * @param namespaceName 使用する値保存用NVS名前空間
 * @param metadataNamespaceName 使用する型情報保存用NVS名前空間
 */
PreferenceCommands::PreferenceCommands(
    const char* namespaceName,
    const char* metadataNamespaceName)
    : m_namespace(namespaceName == nullptr ? "" : namespaceName),
      m_metadataNamespace(
          metadataNamespaceName == nullptr ? "" : metadataNamespaceName),
      m_commands(
          {
              {"pref", "Read and write NVS preferences", CommandPreference, this},
          })
{
}

/**
 * @brief Preferencesの使用を終了します。
 */
PreferenceCommands::~PreferenceCommands()
{
    if (m_started)
    {
        m_preferences.end();
        m_metadataPreferences.end();
    }
}

/**
 * @brief NVS名前空間を読み書き可能な状態で開きます。
 *
 * @return 名前空間を開けた場合はtrue、それ以外はfalse
 */
bool PreferenceCommands::Begin()
{
    if (m_started)
    {
        return true;
    }

    if (m_namespace.empty() ||
        m_namespace.length() > MaxNvsNameLength ||
        m_metadataNamespace.empty() ||
        m_metadataNamespace.length() > MaxNvsNameLength)
    {
        return false;
    }

    if (!m_preferences.begin(m_namespace.c_str(), false))
    {
        return false;
    }

    if (!m_metadataPreferences.begin(m_metadataNamespace.c_str(), false))
    {
        m_preferences.end();
        return false;
    }

    m_started = true;
    return m_started;
}

/**
 * @brief NT-Shellへ登録するコマンド一覧を取得します。
 *
 * @return NT-Shellコマンド一覧
 */
const std::vector<NtShell::Command>& PreferenceCommands::GetCommands() const
{
    return m_commands;
}

/**
 * @brief NT-Shellのコールバックを対象インスタンスへ転送します。
 *
 * @param output コマンド結果の出力先
 * @param argc コマンド名を含む引数の個数
 * @param argv コマンド名を先頭に格納した引数配列
 * @param context PreferenceCommandsインスタンス
 */
void PreferenceCommands::CommandPreference(
    Stream& output,
    int argc,
    char* argv[],
    void* context)
{
    auto* self = static_cast<PreferenceCommands*>(context);
    self->Execute(output, argc, argv);
}

/**
 * @brief prefコマンドのサブコマンドを解析して実行します。
 *
 * @param output コマンド結果の出力先
 * @param argc コマンド名を含む引数の個数
 * @param argv コマンド名を先頭に格納した引数配列
 */
void PreferenceCommands::Execute(Stream& output, int argc, char* argv[])
{
    if (argc < 2 || strcmp(argv[1], "help") == 0)
    {
        PrintHelp(output);
        return;
    }

    if (!EnsureStarted(output))
    {
        return;
    }

    if (strcmp(argv[1], "status") == 0 && argc == 2)
    {
        String response("OK namespace=");
        response += m_namespace.c_str();
        response += " free_entries=";
        response += m_preferences.freeEntries();
        WriteLine(output, response);
        return;
    }

    if (strcmp(argv[1], "list") == 0 && argc == 2)
    {
        ListValues(output);
        return;
    }

    if (strcmp(argv[1], "exists") == 0 && argc == 3)
    {
        if (!ValidateKey(output, argv[2]))
        {
            return;
        }

        String response("OK ");
        response += argv[2];
        response += ' ';
        response += m_preferences.isKey(argv[2]) ? "true" : "false";
        WriteLine(output, response);
        return;
    }

    if (strcmp(argv[1], "get") == 0 && argc == 4)
    {
        if (ValidateKey(output, argv[3]))
        {
            GetValue(output, argv[2], argv[3]);
        }
        return;
    }

    if (strcmp(argv[1], "set") == 0 && argc >= 5)
    {
        if (!ValidateKey(output, argv[3]))
        {
            return;
        }

        String value(argv[4]);
        for (int index = 5; index < argc; ++index)
        {
            value += ' ';
            value += argv[index];
        }
        SetValue(output, argv[2], argv[3], value);
        return;
    }

    if (strcmp(argv[1], "remove") == 0 && argc == 3)
    {
        if (!ValidateKey(output, argv[2]))
        {
            return;
        }

        const bool valueRemoved = m_preferences.remove(argv[2]);
        const bool metadataRemoved =
            !m_metadataPreferences.isKey(argv[2]) ||
            m_metadataPreferences.remove(argv[2]);
        WriteLine(
            output,
            valueRemoved && metadataRemoved
                ? "OK removed"
                : "ERROR remove_failed");
        return;
    }

    if (strcmp(argv[1], "clear") == 0 && argc == 3 && strcmp(argv[2], "YES") == 0)
    {
        const bool valuesCleared = m_preferences.clear();
        const bool metadataCleared = m_metadataPreferences.clear();
        WriteLine(
            output,
            valuesCleared && metadataCleared
                ? "OK cleared"
                : "ERROR clear_failed");
        return;
    }

    WriteLine(output, "ERROR invalid_arguments");
    PrintHelp(output);
}

/**
 * @brief 指定された型で設定値を読み出して表示します。
 *
 * @param output コマンド結果の出力先
 * @param typeName 読み出す値の型名
 * @param key 読み出すキー
 */
void PreferenceCommands::GetValue(
    Stream& output,
    const char* typeName,
    const char* key)
{
    if (!m_preferences.isKey(key))
    {
        WriteLine(output, "ERROR key_not_found");
        return;
    }

    const PreferenceType storedType = m_preferences.getType(key);
    const EnStoredType requestedType = ParseStoredType(typeName);
    const EnStoredType metadataType = GetStoredType(m_metadataPreferences, key);

    if (requestedType == EnStoredType::Unknown)
    {
        WriteLine(output, "ERROR invalid_type");
        return;
    }

    if (metadataType == EnStoredType::Unknown)
    {
        WriteLine(output, "ERROR type_metadata_not_found");
        return;
    }

    if (metadataType != requestedType)
    {
        WriteLine(output, "ERROR type_mismatch");
        return;
    }

    const bool isCompatibleType =
        (strcmp(typeName, "bool") == 0 && storedType == PT_U8) ||
        (strcmp(typeName, "i8") == 0 && storedType == PT_I8) ||
        (strcmp(typeName, "u8") == 0 && storedType == PT_U8) ||
        (strcmp(typeName, "i16") == 0 && storedType == PT_I16) ||
        (strcmp(typeName, "u16") == 0 && storedType == PT_U16) ||
        (strcmp(typeName, "i32") == 0 && storedType == PT_I32) ||
        (strcmp(typeName, "u32") == 0 && storedType == PT_U32) ||
        (strcmp(typeName, "i64") == 0 && storedType == PT_I64) ||
        (strcmp(typeName, "u64") == 0 && storedType == PT_U64) ||
        (strcmp(typeName, "float") == 0 &&
         storedType == PT_BLOB &&
         m_preferences.getBytesLength(key) == sizeof(float_t)) ||
        (strcmp(typeName, "double") == 0 &&
         storedType == PT_BLOB &&
         m_preferences.getBytesLength(key) == sizeof(double_t)) ||
        (strcmp(typeName, "string") == 0 && storedType == PT_STR);

    if (!isCompatibleType)
    {
        WriteLine(output, "ERROR type_mismatch");
        return;
    }

    if (strcmp(typeName, "bool") == 0 && m_preferences.getUChar(key) > 1)
    {
        WriteLine(output, "ERROR invalid_boolean");
        return;
    }

    String response("OK ");
    response += key;
    response += ' ';
    response += typeName;
    response += ' ';

    if (strcmp(typeName, "bool") == 0)
    {
        response += m_preferences.getBool(key) ? "true" : "false";
    }
    else if (strcmp(typeName, "i8") == 0)
    {
        AppendSigned(response, m_preferences.getChar(key));
    }
    else if (strcmp(typeName, "u8") == 0)
    {
        AppendUnsigned(response, m_preferences.getUChar(key));
    }
    else if (strcmp(typeName, "i16") == 0)
    {
        AppendSigned(response, m_preferences.getShort(key));
    }
    else if (strcmp(typeName, "u16") == 0)
    {
        AppendUnsigned(response, m_preferences.getUShort(key));
    }
    else if (strcmp(typeName, "i32") == 0)
    {
        AppendSigned(response, m_preferences.getInt(key));
    }
    else if (strcmp(typeName, "u32") == 0)
    {
        AppendUnsigned(response, m_preferences.getUInt(key));
    }
    else if (strcmp(typeName, "i64") == 0)
    {
        AppendSigned(response, m_preferences.getLong64(key));
    }
    else if (strcmp(typeName, "u64") == 0)
    {
        AppendUnsigned(response, m_preferences.getULong64(key));
    }
    else if (strcmp(typeName, "float") == 0)
    {
        response += String(m_preferences.getFloat(key), 6);
    }
    else if (strcmp(typeName, "double") == 0)
    {
        response += String(m_preferences.getDouble(key), 10);
    }
    else if (strcmp(typeName, "string") == 0)
    {
        response += m_preferences.getString(key);
    }

    WriteLine(output, response);
}

/**
 * @brief 指定された型で設定値を保存します。
 *
 * @param output コマンド結果の出力先
 * @param typeName 保存する値の型名
 * @param key 保存するキー
 * @param value 保存する文字列表現
 */
void PreferenceCommands::SetValue(
    Stream& output,
    const char* typeName,
    const char* key,
    const String& value)
{
    size_t writtenSize = 0;
    const EnStoredType storedType = ParseStoredType(typeName);
    int64_t signedValue = 0;
    uint64_t unsignedValue = 0;
    double floatingPointValue = 0.0;

    if (strcmp(typeName, "bool") == 0)
    {
        if (value == "true" || value == "1")
        {
            writtenSize = m_preferences.putBool(key, true);
        }
        else if (value == "false" || value == "0")
        {
            writtenSize = m_preferences.putBool(key, false);
        }
        else
        {
            WriteLine(output, "ERROR invalid_value");
            return;
        }
    }
    else if (strcmp(typeName, "i8") == 0 &&
             ParseSigned(value.c_str(), signedValue) &&
             signedValue >= INT8_MIN &&
             signedValue <= INT8_MAX)
    {
        writtenSize = m_preferences.putChar(key, static_cast<int8_t>(signedValue));
    }
    else if (strcmp(typeName, "u8") == 0 &&
             ParseUnsigned(value.c_str(), unsignedValue) &&
             unsignedValue <= UINT8_MAX)
    {
        writtenSize = m_preferences.putUChar(key, static_cast<uint8_t>(unsignedValue));
    }
    else if (strcmp(typeName, "i16") == 0 &&
             ParseSigned(value.c_str(), signedValue) &&
             signedValue >= INT16_MIN &&
             signedValue <= INT16_MAX)
    {
        writtenSize = m_preferences.putShort(key, static_cast<int16_t>(signedValue));
    }
    else if (strcmp(typeName, "u16") == 0 &&
             ParseUnsigned(value.c_str(), unsignedValue) &&
             unsignedValue <= UINT16_MAX)
    {
        writtenSize = m_preferences.putUShort(key, static_cast<uint16_t>(unsignedValue));
    }
    else if (strcmp(typeName, "i32") == 0 &&
             ParseSigned(value.c_str(), signedValue) &&
             signedValue >= INT32_MIN &&
             signedValue <= INT32_MAX)
    {
        writtenSize = m_preferences.putInt(key, static_cast<int32_t>(signedValue));
    }
    else if (strcmp(typeName, "u32") == 0 &&
             ParseUnsigned(value.c_str(), unsignedValue) &&
             unsignedValue <= UINT32_MAX)
    {
        writtenSize = m_preferences.putUInt(key, static_cast<uint32_t>(unsignedValue));
    }
    else if (strcmp(typeName, "i64") == 0 && ParseSigned(value.c_str(), signedValue))
    {
        writtenSize = m_preferences.putLong64(key, signedValue);
    }
    else if (strcmp(typeName, "u64") == 0 && ParseUnsigned(value.c_str(), unsignedValue))
    {
        writtenSize = m_preferences.putULong64(key, unsignedValue);
    }
    else if (strcmp(typeName, "float") == 0 &&
             ParseFloatingPoint(value.c_str(), floatingPointValue) &&
             floatingPointValue >= -FLT_MAX &&
             floatingPointValue <= FLT_MAX)
    {
        writtenSize = m_preferences.putFloat(key, static_cast<float>(floatingPointValue));
    }
    else if (strcmp(typeName, "double") == 0 &&
             ParseFloatingPoint(value.c_str(), floatingPointValue))
    {
        writtenSize = m_preferences.putDouble(key, floatingPointValue);
    }
    else if (strcmp(typeName, "string") == 0)
    {
        writtenSize = m_preferences.putString(key, value);
    }
    else
    {
        WriteLine(output, "ERROR invalid_type_or_value");
        return;
    }

    if (writtenSize == 0)
    {
        WriteLine(output, "ERROR save_failed");
        return;
    }

    const size_t metadataSize = m_metadataPreferences.putUChar(
        key,
        static_cast<uint8_t>(storedType));
    if (metadataSize == 0)
    {
        m_preferences.remove(key);
        WriteLine(output, "ERROR metadata_save_failed");
        return;
    }

    WriteLine(output, "OK saved");
}

/**
 * @brief 現在のNVS名前空間に保存されている設定値を一覧表示します。
 *
 * @param output コマンド結果の出力先
 */
void PreferenceCommands::ListValues(Stream& output)
{
    size_t itemCount = 0;
    size_t errorCount = 0;
    nvs_iterator_t iterator = nvs_entry_find(
        "nvs",
        m_namespace.c_str(),
        NVS_TYPE_ANY);

    while (iterator != nullptr)
    {
        nvs_entry_info_t info{};
        nvs_entry_info(iterator, &info);
        const EnStoredType storedType = GetStoredType(
            m_metadataPreferences,
            info.key);

        const bool metadataMatchesValue =
            (storedType == EnStoredType::Bool && info.type == NVS_TYPE_U8) ||
            (storedType == EnStoredType::I8 && info.type == NVS_TYPE_I8) ||
            (storedType == EnStoredType::U8 && info.type == NVS_TYPE_U8) ||
            (storedType == EnStoredType::I16 && info.type == NVS_TYPE_I16) ||
            (storedType == EnStoredType::U16 && info.type == NVS_TYPE_U16) ||
            (storedType == EnStoredType::I32 && info.type == NVS_TYPE_I32) ||
            (storedType == EnStoredType::U32 && info.type == NVS_TYPE_U32) ||
            (storedType == EnStoredType::I64 && info.type == NVS_TYPE_I64) ||
            (storedType == EnStoredType::U64 && info.type == NVS_TYPE_U64) ||
            (storedType == EnStoredType::Float &&
             info.type == NVS_TYPE_BLOB &&
             m_preferences.getBytesLength(info.key) == sizeof(float_t)) ||
            (storedType == EnStoredType::Double &&
             info.type == NVS_TYPE_BLOB &&
             m_preferences.getBytesLength(info.key) == sizeof(double_t)) ||
            (storedType == EnStoredType::String && info.type == NVS_TYPE_STR);

        if (storedType == EnStoredType::Unknown)
        {
            String line("ERROR ");
            line += info.key;
            line += " type_metadata_not_found";
            WriteLine(output, line);
            ++errorCount;
        }
        else if (!metadataMatchesValue)
        {
            String line("ERROR ");
            line += info.key;
            line += " invalid_type_metadata";
            WriteLine(output, line);
            ++errorCount;
        }
        else if (storedType == EnStoredType::Bool &&
                 m_preferences.getUChar(info.key) > 1)
        {
            String line("ERROR ");
            line += info.key;
            line += " invalid_boolean";
            WriteLine(output, line);
            ++errorCount;
        }
        else
        {
            String line("ITEM ");
            line += info.key;
            line += ' ';
            line += GetStoredTypeName(storedType);
            line += ' ';

            switch (storedType)
            {
            case EnStoredType::Bool:
                line += m_preferences.getBool(info.key) ? "true" : "false";
                break;
            case EnStoredType::I8:
                AppendSigned(line, m_preferences.getChar(info.key));
                break;
            case EnStoredType::I16:
                AppendSigned(line, m_preferences.getShort(info.key));
                break;
            case EnStoredType::I32:
                AppendSigned(line, m_preferences.getInt(info.key));
                break;
            case EnStoredType::I64:
                AppendSigned(line, m_preferences.getLong64(info.key));
                break;
            case EnStoredType::U8:
                AppendUnsigned(line, m_preferences.getUChar(info.key));
                break;
            case EnStoredType::U16:
                AppendUnsigned(line, m_preferences.getUShort(info.key));
                break;
            case EnStoredType::U32:
                AppendUnsigned(line, m_preferences.getUInt(info.key));
                break;
            case EnStoredType::U64:
                AppendUnsigned(line, m_preferences.getULong64(info.key));
                break;
            case EnStoredType::Float:
                line += String(m_preferences.getFloat(info.key), 6);
                break;
            case EnStoredType::Double:
                line += String(m_preferences.getDouble(info.key), 10);
                break;
            case EnStoredType::String:
                line += m_preferences.getString(info.key);
                break;
            default:
                break;
            }

            WriteLine(output, line);
            ++itemCount;
        }

        iterator = nvs_entry_next(iterator);
    }

    String summary(errorCount == 0 ? "OK count=" : "ERROR count=");
    summary += itemCount;
    summary += " metadata_errors=";
    summary += errorCount;
    WriteLine(output, summary);
}

/**
 * @brief コマンドの使用方法を表示します。
 *
 * @param output コマンド結果の出力先
 */
void PreferenceCommands::PrintHelp(Stream& output) const
{
    static const char HelpText[] =
        "pref status\r\n"
        "pref list\r\n"
        "pref exists <key>\r\n"
        "pref get <type> <key>\r\n"
        "pref set <type> <key> <value>\r\n"
        "pref remove <key>\r\n"
        "pref clear YES\r\n"
        "types: bool i8 u8 i16 u16 i32 u32 i64 u64 float double string\r\n";
    output.print(HelpText);
}

/**
 * @brief Preferencesを利用できる状態か検査します。
 *
 * @param output エラーの出力先
 * @return 利用できる場合はtrue、それ以外はfalse
 */
bool PreferenceCommands::EnsureStarted(Stream& output) const
{
    if (m_started)
    {
        return true;
    }

    WriteLine(output, "ERROR preferences_not_started");
    return false;
}

/**
 * @brief NVSキーが有効な長さか検査します。
 *
 * @param output エラーの出力先
 * @param key 検査するキー
 * @return 有効な場合はtrue、それ以外はfalse
 */
bool PreferenceCommands::ValidateKey(Stream& output, const char* key) const
{
    if (key != nullptr && key[0] != '\0' && strlen(key) <= MaxNvsNameLength)
    {
        return true;
    }

    WriteLine(output, "ERROR invalid_key");
    return false;
}
