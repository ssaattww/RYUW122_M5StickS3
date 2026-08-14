#include "PreferenceCommands.h"

#include <Arduino.h>

#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    /**
     * @brief 符号付き整数の文字列を64bit整数へ変換します。
     *
     * @param text 変換する文字列
     * @param value 変換結果の格納先
     * @return 文字列全体を変換できた場合はtrue、それ以外はfalse
     */
    bool ParseSigned(const char* text, int64_t& value)
    {
        if (text == nullptr || text[0] == '\0') return false;
        errno = 0;
        char* endPointer = nullptr;
        const long long parsedValue = strtoll(text, &endPointer, 0);
        if (errno == ERANGE || endPointer == text || *endPointer != '\0') return false;
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
        if (text == nullptr || text[0] == '\0' || text[0] == '-') return false;
        errno = 0;
        char* endPointer = nullptr;
        const unsigned long long parsedValue = strtoull(text, &endPointer, 0);
        if (errno == ERANGE || endPointer == text || *endPointer != '\0') return false;
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
        if (text == nullptr || text[0] == '\0') return false;
        errno = 0;
        char* endPointer = nullptr;
        const double parsedValue = strtod(text, &endPointer);
        if (errno == ERANGE || endPointer == text || *endPointer != '\0' ||
            !std::isfinite(parsedValue)) return false;
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
        snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
        output += buffer;
    }

    /**
     * @brief 改行を含む1行を1回のストリーム書き込みで送信します。
     *
     * @param output 出力先
     * @param text 改行を含まない出力文字列
     * @return 送信できた場合はtrue、それ以外はfalse
     */
    bool WriteLine(Stream& output, String text)
    {
        if (!text.reserve(text.length() + 2)) return false;
        text += "\r\n";
        return output.write(
            reinterpret_cast<const uint8_t*>(text.c_str()),
            text.length()) == text.length();
    }

    /**
     * @brief NVS処理結果を既存コマンドのエラー文字列へ変換します。
     *
     * @param result NVS処理結果
     * @return エラー文字列
     */
    const char* GetErrorText(EnNvsResult result)
    {
        switch (result)
        {
        case EnNvsResult::NotStarted: return "ERROR preferences_not_started";
        case EnNvsResult::InvalidNamespace: return "ERROR invalid_namespace";
        case EnNvsResult::InvalidKey: return "ERROR invalid_key";
        case EnNvsResult::NotFound: return "ERROR key_not_found";
        case EnNvsResult::InvalidType: return "ERROR invalid_type";
        case EnNvsResult::TypeMetadataNotFound: return "ERROR type_metadata_not_found";
        case EnNvsResult::TypeMismatch: return "ERROR type_mismatch";
        case EnNvsResult::InvalidBoolean: return "ERROR invalid_boolean";
        case EnNvsResult::ReadFailed: return "ERROR read_failed";
        case EnNvsResult::MetadataSaveFailed: return "ERROR metadata_save_failed";
        case EnNvsResult::RemoveFailed: return "ERROR remove_failed";
        case EnNvsResult::ClearFailed: return "ERROR clear_failed";
        case EnNvsResult::ListFailed: return "ERROR list_failed";
        case EnNvsResult::InvalidValue: return "ERROR invalid_value";
            default: return "ERROR save_failed";
        }
    }

    /**
     * @brief NVS処理結果を`ERROR `を含まないエラー名へ変換します。
     *
     * @param result NVS処理結果
     * @return エラー名
     */
    const char* GetErrorName(EnNvsResult result)
    {
        constexpr size_t ErrorPrefixLength = 6;
        return GetErrorText(result) + ErrorPrefixLength;
    }
}

PreferenceCommands::PreferenceCommands(NvsPreferenceStore& store)
    : m_store(store),
      m_commands(
          {
              {"pref", "Read and write NVS preferences", CommandPreference, this},
          })
{
}

const std::vector<NtShell::Command>& PreferenceCommands::GetCommands() const
{
    return m_commands;
}

void PreferenceCommands::CommandPreference(
    Stream& output,
    int argc,
    char* argv[],
    void* context)
{
    auto* self = static_cast<PreferenceCommands*>(context);
    self->Execute(output, argc, argv);
}

void PreferenceCommands::VisitListEntry(const NvsEntryInfo& entry, void* context)
{
    auto* listContext = static_cast<ListContext*>(context);
    if (entry.m_result == EnNvsResult::Ok)
    {
        const EnNvsResult result = listContext->m_commands->WriteListValue(
            *listContext->m_output,
            entry);
        if (result == EnNvsResult::Ok)
        {
            ++listContext->m_outputItemCount;
            return;
        }

        ++listContext->m_valueErrorCount;
        String line("ERROR ");
        line += entry.m_key;
        line += ' ';
        line += GetErrorName(result);
        WriteLine(*listContext->m_output, line);
        return;
    }

    String line("ERROR ");
    line += entry.m_key;
    if (entry.m_result == EnNvsResult::TypeMetadataNotFound)
    {
        line += " type_metadata_not_found";
    }
    else if (entry.m_result == EnNvsResult::InvalidBoolean)
    {
        line += " invalid_boolean";
    }
    else if (entry.m_result == EnNvsResult::ReadFailed)
    {
        line += " read_failed";
    }
    else
    {
        line += " invalid_type_metadata";
    }
    WriteLine(*listContext->m_output, line);
}

void PreferenceCommands::Execute(Stream& output, int argc, char* argv[])
{
    if (argc < 2 || strcmp(argv[1], "help") == 0)
    {
        PrintHelp(output);
        return;
    }
    if (!EnsureStarted(output)) return;

    if (strcmp(argv[1], "status") == 0 && argc == 2)
    {
        size_t freeEntries = 0;
        const EnNvsResult result = m_store.GetFreeEntries(freeEntries);
        if (result != EnNvsResult::Ok)
        {
            WriteLine(output, GetErrorText(result));
            return;
        }
        String response("OK namespace=");
        response += m_store.GetNamespaceName();
        response += " free_entries=";
        response += freeEntries;
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
        if (!ValidateKey(output, argv[2])) return;
        bool exists = false;
        const EnNvsResult result = m_store.Exists(argv[2], exists);
        if (result != EnNvsResult::Ok)
        {
            WriteLine(output, GetErrorText(result));
            return;
        }
        String response("OK ");
        response += argv[2];
        response += exists ? " true" : " false";
        WriteLine(output, response);
        return;
    }

    if (strcmp(argv[1], "get") == 0 && argc == 4)
    {
        if (ValidateKey(output, argv[3])) GetValue(output, argv[2], argv[3]);
        return;
    }

    if (strcmp(argv[1], "set") == 0 && argc >= 5)
    {
        if (!ValidateKey(output, argv[3])) return;
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
        if (!ValidateKey(output, argv[2])) return;
        const EnNvsResult result = m_store.Remove(argv[2]);
        WriteLine(output, result == EnNvsResult::Ok
            ? "OK removed"
            : "ERROR remove_failed");
        return;
    }

    if (strcmp(argv[1], "clear") == 0 && argc == 3 &&
        strcmp(argv[2], "YES") == 0)
    {
        const EnNvsResult result = m_store.Clear();
        WriteLine(output, result == EnNvsResult::Ok
            ? "OK cleared"
            : GetErrorText(result));
        return;
    }

    WriteLine(output, "ERROR invalid_arguments");
    PrintHelp(output);
}

void PreferenceCommands::GetValue(
    Stream& output,
    const char* typeName,
    const char* key)
{
    EnNvsValueType type = EnNvsValueType::Unknown;
    EnNvsResult result = NvsPreferenceStore::ParseValueType(typeName, type);
    if (result != EnNvsResult::Ok)
    {
        WriteLine(output, GetErrorText(result));
        return;
    }

    String response("OK ");
    response += key;
    response += ' ';
    response += typeName;
    response += ' ';

    bool boolValue = false;
    int8_t i8Value = 0;
    uint8_t u8Value = 0;
    int16_t i16Value = 0;
    uint16_t u16Value = 0;
    int32_t i32Value = 0;
    uint32_t u32Value = 0;
    int64_t i64Value = 0;
    uint64_t u64Value = 0;
    float floatValue = 0.0F;
    double doubleValue = 0.0;
    String stringValue;

    switch (type)
    {
    case EnNvsValueType::Bool:
        result = m_store.GetBool(key, boolValue);
        if (result == EnNvsResult::Ok) response += boolValue ? "true" : "false";
        break;
    case EnNvsValueType::I8:
        result = m_store.GetI8(key, i8Value);
        if (result == EnNvsResult::Ok) AppendSigned(response, i8Value);
        break;
    case EnNvsValueType::U8:
        result = m_store.GetU8(key, u8Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(response, u8Value);
        break;
    case EnNvsValueType::I16:
        result = m_store.GetI16(key, i16Value);
        if (result == EnNvsResult::Ok) AppendSigned(response, i16Value);
        break;
    case EnNvsValueType::U16:
        result = m_store.GetU16(key, u16Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(response, u16Value);
        break;
    case EnNvsValueType::I32:
        result = m_store.GetI32(key, i32Value);
        if (result == EnNvsResult::Ok) AppendSigned(response, i32Value);
        break;
    case EnNvsValueType::U32:
        result = m_store.GetU32(key, u32Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(response, u32Value);
        break;
    case EnNvsValueType::I64:
        result = m_store.GetI64(key, i64Value);
        if (result == EnNvsResult::Ok) AppendSigned(response, i64Value);
        break;
    case EnNvsValueType::U64:
        result = m_store.GetU64(key, u64Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(response, u64Value);
        break;
    case EnNvsValueType::Float:
        result = m_store.GetFloat(key, floatValue);
        if (result == EnNvsResult::Ok) response += String(floatValue, 6);
        break;
    case EnNvsValueType::Double:
        result = m_store.GetDouble(key, doubleValue);
        if (result == EnNvsResult::Ok) response += String(doubleValue, 10);
        break;
    case EnNvsValueType::String:
        result = m_store.GetString(key, stringValue);
        if (result == EnNvsResult::Ok) response += stringValue;
        break;
    default:
        result = EnNvsResult::InvalidType;
        break;
    }

    WriteLine(output, result == EnNvsResult::Ok ? response : GetErrorText(result));
}

void PreferenceCommands::SetValue(
    Stream& output,
    const char* typeName,
    const char* key,
    const String& value)
{
    EnNvsValueType type = EnNvsValueType::Unknown;
    EnNvsResult result = NvsPreferenceStore::ParseValueType(typeName, type);
    if (result != EnNvsResult::Ok)
    {
        WriteLine(output, "ERROR invalid_type_or_value");
        return;
    }

    int64_t signedValue = 0;
    uint64_t unsignedValue = 0;
    double floatingPointValue = 0.0;
    switch (type)
    {
    case EnNvsValueType::Bool:
        if (value == "true" || value == "1") result = m_store.SetBool(key, true);
        else if (value == "false" || value == "0") result = m_store.SetBool(key, false);
        else
        {
            WriteLine(output, "ERROR invalid_value");
            return;
        }
        break;
    case EnNvsValueType::I8:
        result = ParseSigned(value.c_str(), signedValue) &&
            signedValue >= INT8_MIN && signedValue <= INT8_MAX
            ? m_store.SetI8(key, static_cast<int8_t>(signedValue))
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::U8:
        result = ParseUnsigned(value.c_str(), unsignedValue) && unsignedValue <= UINT8_MAX
            ? m_store.SetU8(key, static_cast<uint8_t>(unsignedValue))
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::I16:
        result = ParseSigned(value.c_str(), signedValue) &&
            signedValue >= INT16_MIN && signedValue <= INT16_MAX
            ? m_store.SetI16(key, static_cast<int16_t>(signedValue))
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::U16:
        result = ParseUnsigned(value.c_str(), unsignedValue) && unsignedValue <= UINT16_MAX
            ? m_store.SetU16(key, static_cast<uint16_t>(unsignedValue))
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::I32:
        result = ParseSigned(value.c_str(), signedValue) &&
            signedValue >= INT32_MIN && signedValue <= INT32_MAX
            ? m_store.SetI32(key, static_cast<int32_t>(signedValue))
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::U32:
        result = ParseUnsigned(value.c_str(), unsignedValue) && unsignedValue <= UINT32_MAX
            ? m_store.SetU32(key, static_cast<uint32_t>(unsignedValue))
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::I64:
        result = ParseSigned(value.c_str(), signedValue)
            ? m_store.SetI64(key, signedValue)
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::U64:
        result = ParseUnsigned(value.c_str(), unsignedValue)
            ? m_store.SetU64(key, unsignedValue)
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::Float:
        result = ParseFloatingPoint(value.c_str(), floatingPointValue) &&
            floatingPointValue >= -FLT_MAX && floatingPointValue <= FLT_MAX
            ? m_store.SetFloat(key, static_cast<float>(floatingPointValue))
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::Double:
        result = ParseFloatingPoint(value.c_str(), floatingPointValue)
            ? m_store.SetDouble(key, floatingPointValue)
            : EnNvsResult::InvalidValue;
        break;
    case EnNvsValueType::String:
        result = m_store.SetString(key, value);
        break;
    default:
        result = EnNvsResult::InvalidValue;
        break;
    }

    if (result == EnNvsResult::Ok) WriteLine(output, "OK saved");
    else if (result == EnNvsResult::InvalidValue) WriteLine(output, "ERROR invalid_type_or_value");
    else WriteLine(output, GetErrorText(result));
}

void PreferenceCommands::ListValues(Stream& output)
{
    ListContext context{&output, this, 0, 0};
    size_t itemCount = 0;
    size_t errorCount = 0;
    const EnNvsResult result = m_store.List(
        VisitListEntry,
        &context,
        itemCount,
        errorCount);
    if (result != EnNvsResult::Ok)
    {
        WriteLine(output, GetErrorText(result));
        return;
    }

    const bool countsMatch = itemCount ==
        context.m_outputItemCount + context.m_valueErrorCount;
    const bool hasError = errorCount != 0 ||
        context.m_valueErrorCount != 0 ||
        !countsMatch;
    String summary(hasError ? "ERROR count=" : "OK count=");
    summary += context.m_outputItemCount;
    summary += " metadata_errors=";
    summary += errorCount;
    summary += " read_errors=";
    summary += context.m_valueErrorCount;
    WriteLine(output, summary);
}

EnNvsResult PreferenceCommands::WriteListValue(
    Stream& output,
    const NvsEntryInfo& entry)
{
    String line("ITEM ");
    line += entry.m_key;
    line += ' ';
    line += NvsPreferenceStore::GetValueTypeName(entry.m_type);
    line += ' ';

    bool boolValue = false;
    int8_t i8Value = 0;
    uint8_t u8Value = 0;
    int16_t i16Value = 0;
    uint16_t u16Value = 0;
    int32_t i32Value = 0;
    uint32_t u32Value = 0;
    int64_t i64Value = 0;
    uint64_t u64Value = 0;
    float floatValue = 0.0F;
    double doubleValue = 0.0;
    String stringValue;
    EnNvsResult result = EnNvsResult::InvalidType;

    switch (entry.m_type)
    {
    case EnNvsValueType::Bool:
        result = m_store.GetBool(entry.m_key, boolValue);
        if (result == EnNvsResult::Ok)
        {
            line += boolValue ? "true" : "false";
        }
        break;
    case EnNvsValueType::I8:
        result = m_store.GetI8(entry.m_key, i8Value);
        if (result == EnNvsResult::Ok) AppendSigned(line, i8Value);
        break;
    case EnNvsValueType::U8:
        result = m_store.GetU8(entry.m_key, u8Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(line, u8Value);
        break;
    case EnNvsValueType::I16:
        result = m_store.GetI16(entry.m_key, i16Value);
        if (result == EnNvsResult::Ok) AppendSigned(line, i16Value);
        break;
    case EnNvsValueType::U16:
        result = m_store.GetU16(entry.m_key, u16Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(line, u16Value);
        break;
    case EnNvsValueType::I32:
        result = m_store.GetI32(entry.m_key, i32Value);
        if (result == EnNvsResult::Ok) AppendSigned(line, i32Value);
        break;
    case EnNvsValueType::U32:
        result = m_store.GetU32(entry.m_key, u32Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(line, u32Value);
        break;
    case EnNvsValueType::I64:
        result = m_store.GetI64(entry.m_key, i64Value);
        if (result == EnNvsResult::Ok) AppendSigned(line, i64Value);
        break;
    case EnNvsValueType::U64:
        result = m_store.GetU64(entry.m_key, u64Value);
        if (result == EnNvsResult::Ok) AppendUnsigned(line, u64Value);
        break;
    case EnNvsValueType::Float:
        result = m_store.GetFloat(entry.m_key, floatValue);
        if (result == EnNvsResult::Ok) line += String(floatValue, 6);
        break;
    case EnNvsValueType::Double:
        result = m_store.GetDouble(entry.m_key, doubleValue);
        if (result == EnNvsResult::Ok) line += String(doubleValue, 10);
        break;
    case EnNvsValueType::String:
        result = m_store.GetString(entry.m_key, stringValue);
        if (result == EnNvsResult::Ok) line += stringValue;
        break;
    default:
        return EnNvsResult::InvalidType;
    }
    if (result != EnNvsResult::Ok) return result;
    WriteLine(output, line);
    return EnNvsResult::Ok;
}

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

bool PreferenceCommands::EnsureStarted(Stream& output) const
{
    if (m_store.IsStarted()) return true;
    WriteLine(output, "ERROR preferences_not_started");
    return false;
}

bool PreferenceCommands::ValidateKey(Stream& output, const char* key) const
{
    if (m_store.ValidateKey(key) == EnNvsResult::Ok) return true;
    WriteLine(output, "ERROR invalid_key");
    return false;
}
