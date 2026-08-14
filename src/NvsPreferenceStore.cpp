#include "NvsPreferenceStore.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <utility>

namespace
{
    constexpr size_t MaxNvsNameLength = 15;
}

NvsPreferenceStore::NvsPreferenceStore(
    const char* namespaceName,
    const char* metadataNamespaceName)
    : m_namespace(namespaceName == nullptr ? "" : namespaceName),
      m_metadataNamespace(
          metadataNamespaceName == nullptr ? "" : metadataNamespaceName)
{
}

NvsPreferenceStore::~NvsPreferenceStore()
{
    End();
}

EnNvsResult NvsPreferenceStore::Begin()
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_started)
    {
        return EnNvsResult::Ok;
    }

    if (m_namespace.empty() ||
        m_namespace.length() > MaxNvsNameLength ||
        m_metadataNamespace.empty() ||
        m_metadataNamespace.length() > MaxNvsNameLength ||
        m_namespace == m_metadataNamespace)
    {
        return EnNvsResult::InvalidNamespace;
    }

    if (nvs_open(
            m_namespace.c_str(),
            NVS_READWRITE,
            &m_valueHandle) != ESP_OK)
    {
        m_valueHandle = 0;
        return EnNvsResult::SaveFailed;
    }

    if (nvs_open(
            m_metadataNamespace.c_str(),
            NVS_READWRITE,
            &m_metadataHandle) != ESP_OK)
    {
        nvs_close(m_valueHandle);
        m_valueHandle = 0;
        m_metadataHandle = 0;
        return EnNvsResult::SaveFailed;
    }

    m_started = true;
    return EnNvsResult::Ok;
}

void NvsPreferenceStore::End()
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started)
    {
        return;
    }

    nvs_close(m_metadataHandle);
    nvs_close(m_valueHandle);
    m_metadataHandle = 0;
    m_valueHandle = 0;
    m_started = false;
}

bool NvsPreferenceStore::IsStarted() const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_started;
}

const char* NvsPreferenceStore::GetNamespaceName() const
{
    return m_namespace.c_str();
}

const char* NvsPreferenceStore::GetMetadataNamespaceName() const
{
    return m_metadataNamespace.c_str();
}

EnNvsResult NvsPreferenceStore::ValidateKey(const char* key) const
{
    if (key == nullptr || key[0] == '\0' || strlen(key) > MaxNvsNameLength)
    {
        return EnNvsResult::InvalidKey;
    }

    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::Exists(const char* key, bool& exists) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started)
    {
        return EnNvsResult::NotStarted;
    }

    const EnNvsResult validationResult = ValidateKey(key);
    if (validationResult != EnNvsResult::Ok)
    {
        return validationResult;
    }

    nvs_type_t storedType = NVS_TYPE_ANY;
    const esp_err_t findResult = nvs_find_key(m_valueHandle, key, &storedType);
    if (findResult == ESP_ERR_NVS_NOT_FOUND)
    {
        exists = false;
        return EnNvsResult::Ok;
    }
    if (findResult != ESP_OK)
    {
        return EnNvsResult::ReadFailed;
    }

    exists = true;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetFreeEntries(size_t& count) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started)
    {
        return EnNvsResult::NotStarted;
    }

    nvs_stats_t stats{};
    if (nvs_get_stats(nullptr, &stats) != ESP_OK)
    {
        return EnNvsResult::ReadFailed;
    }

    count = stats.free_entries;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetValueType(
    const char* key,
    EnNvsValueType& type) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started)
    {
        return EnNvsResult::NotStarted;
    }

    const EnNvsResult validationResult = ValidateKey(key);
    if (validationResult != EnNvsResult::Ok)
    {
        return validationResult;
    }

    nvs_type_t rawValueType = NVS_TYPE_ANY;
    const esp_err_t findResult = nvs_find_key(
        m_valueHandle,
        key,
        &rawValueType);
    if (findResult == ESP_ERR_NVS_NOT_FOUND)
    {
        return EnNvsResult::NotFound;
    }
    if (findResult != ESP_OK)
    {
        return EnNvsResult::ReadFailed;
    }

    EnNvsValueType storedType = EnNvsValueType::Unknown;
    const EnNvsResult metadataResult = GetMetadataType(key, storedType);
    if (metadataResult != EnNvsResult::Ok)
    {
        return metadataResult;
    }

    const EnNvsResult typeValidationResult = ValidateStoredValue(key, storedType);
    if (typeValidationResult != EnNvsResult::Ok)
    {
        return typeValidationResult;
    }

    type = storedType;
    return EnNvsResult::Ok;
}

const char* NvsPreferenceStore::GetValueTypeName(EnNvsValueType type)
{
    switch (type)
    {
    case EnNvsValueType::Bool: return "bool";
    case EnNvsValueType::I8: return "i8";
    case EnNvsValueType::U8: return "u8";
    case EnNvsValueType::I16: return "i16";
    case EnNvsValueType::U16: return "u16";
    case EnNvsValueType::I32: return "i32";
    case EnNvsValueType::U32: return "u32";
    case EnNvsValueType::I64: return "i64";
    case EnNvsValueType::U64: return "u64";
    case EnNvsValueType::Float: return "float";
    case EnNvsValueType::Double: return "double";
    case EnNvsValueType::String: return "string";
    default: return "unknown";
    }
}

EnNvsResult NvsPreferenceStore::ParseValueType(
    const char* typeName,
    EnNvsValueType& type)
{
    if (typeName == nullptr)
    {
        return EnNvsResult::InvalidType;
    }

    EnNvsValueType parsedType = EnNvsValueType::Unknown;
    if (strcmp(typeName, "bool") == 0) parsedType = EnNvsValueType::Bool;
    else if (strcmp(typeName, "i8") == 0) parsedType = EnNvsValueType::I8;
    else if (strcmp(typeName, "u8") == 0) parsedType = EnNvsValueType::U8;
    else if (strcmp(typeName, "i16") == 0) parsedType = EnNvsValueType::I16;
    else if (strcmp(typeName, "u16") == 0) parsedType = EnNvsValueType::U16;
    else if (strcmp(typeName, "i32") == 0) parsedType = EnNvsValueType::I32;
    else if (strcmp(typeName, "u32") == 0) parsedType = EnNvsValueType::U32;
    else if (strcmp(typeName, "i64") == 0) parsedType = EnNvsValueType::I64;
    else if (strcmp(typeName, "u64") == 0) parsedType = EnNvsValueType::U64;
    else if (strcmp(typeName, "float") == 0) parsedType = EnNvsValueType::Float;
    else if (strcmp(typeName, "double") == 0) parsedType = EnNvsValueType::Double;
    else if (strcmp(typeName, "string") == 0) parsedType = EnNvsValueType::String;
    else return EnNvsResult::InvalidType;

    type = parsedType;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetBool(const char* key, bool& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::Bool);
    if (result != EnNvsResult::Ok) return result;
    uint8_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_u8(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    if (storedValue > 1) return EnNvsResult::InvalidBoolean;
    value = storedValue == 1;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetI8(const char* key, int8_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::I8);
    if (result != EnNvsResult::Ok) return result;
    int8_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_i8(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetU8(const char* key, uint8_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::U8);
    if (result != EnNvsResult::Ok) return result;
    uint8_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_u8(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetI16(const char* key, int16_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::I16);
    if (result != EnNvsResult::Ok) return result;
    int16_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_i16(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetU16(const char* key, uint16_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::U16);
    if (result != EnNvsResult::Ok) return result;
    uint16_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_u16(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetI32(const char* key, int32_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::I32);
    if (result != EnNvsResult::Ok) return result;
    int32_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_i32(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetU32(const char* key, uint32_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::U32);
    if (result != EnNvsResult::Ok) return result;
    uint32_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_u32(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetI64(const char* key, int64_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::I64);
    if (result != EnNvsResult::Ok) return result;
    int64_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_i64(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetU64(const char* key, uint64_t& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::U64);
    if (result != EnNvsResult::Ok) return result;
    uint64_t storedValue = 0;
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_u64(m_valueHandle, key, &storedValue));
    if (readResult != EnNvsResult::Ok) return readResult;
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetFloat(const char* key, float& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::Float);
    if (result != EnNvsResult::Ok) return result;
    float storedValue = 0.0F;
    size_t length = sizeof(storedValue);
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_blob(m_valueHandle, key, &storedValue, &length));
    if (readResult != EnNvsResult::Ok || length != sizeof(storedValue))
    {
        return readResult == EnNvsResult::Ok
            ? EnNvsResult::TypeMismatch
            : readResult;
    }
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetDouble(const char* key, double& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::Double);
    if (result != EnNvsResult::Ok) return result;
    double storedValue = 0.0;
    size_t length = sizeof(storedValue);
    const EnNvsResult readResult = ConvertReadResult(
        nvs_get_blob(m_valueHandle, key, &storedValue, &length));
    if (readResult != EnNvsResult::Ok || length != sizeof(storedValue))
    {
        return readResult == EnNvsResult::Ok
            ? EnNvsResult::TypeMismatch
            : readResult;
    }
    value = storedValue;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetString(const char* key, String& value) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const EnNvsResult result = ValidateRead(key, EnNvsValueType::String);
    if (result != EnNvsResult::Ok) return result;

    size_t length = 0;
    EnNvsResult readResult = ConvertReadResult(
        nvs_get_str(m_valueHandle, key, nullptr, &length));
    if (readResult != EnNvsResult::Ok || length == 0)
    {
        return readResult == EnNvsResult::Ok
            ? EnNvsResult::ReadFailed
            : readResult;
    }

    const std::unique_ptr<char[]> buffer(new (std::nothrow) char[length]);
    if (!buffer)
    {
        return EnNvsResult::ReadFailed;
    }
    readResult = ConvertReadResult(
        nvs_get_str(m_valueHandle, key, buffer.get(), &length));
    if (readResult != EnNvsResult::Ok) return readResult;

    String storedValue;
    const size_t valueLength = length - 1;
    if (!storedValue.reserve(valueLength) ||
        !storedValue.concat(buffer.get(), valueLength))
    {
        return EnNvsResult::ReadFailed;
    }
    value = std::move(storedValue);
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::SetBool(const char* key, bool value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(
        key,
        EnNvsValueType::Bool,
        nvs_set_u8(m_valueHandle, key, value ? 1 : 0));
}

EnNvsResult NvsPreferenceStore::SetI8(const char* key, int8_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::I8, nvs_set_i8(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetU8(const char* key, uint8_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::U8, nvs_set_u8(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetI16(const char* key, int16_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::I16, nvs_set_i16(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetU16(const char* key, uint16_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::U16, nvs_set_u16(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetI32(const char* key, int32_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::I32, nvs_set_i32(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetU32(const char* key, uint32_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::U32, nvs_set_u32(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetI64(const char* key, int64_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::I64, nvs_set_i64(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetU64(const char* key, uint64_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(key, EnNvsValueType::U64, nvs_set_u64(m_valueHandle, key, value));
}

EnNvsResult NvsPreferenceStore::SetFloat(const char* key, float value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    if (!std::isfinite(value)) return EnNvsResult::InvalidValue;
    return FinishSet(
        key,
        EnNvsValueType::Float,
        nvs_set_blob(m_valueHandle, key, &value, sizeof(value)));
}

EnNvsResult NvsPreferenceStore::SetDouble(const char* key, double value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    if (!std::isfinite(value)) return EnNvsResult::InvalidValue;
    return FinishSet(
        key,
        EnNvsValueType::Double,
        nvs_set_blob(m_valueHandle, key, &value, sizeof(value)));
}

EnNvsResult NvsPreferenceStore::SetString(const char* key, const String& value)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult result = ValidateKey(key);
    if (result != EnNvsResult::Ok) return result;
    return FinishSet(
        key,
        EnNvsValueType::String,
        nvs_set_str(m_valueHandle, key, value.c_str()));
}

EnNvsResult NvsPreferenceStore::Remove(const char* key)
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult validationResult = ValidateKey(key);
    if (validationResult != EnNvsResult::Ok) return validationResult;

    nvs_type_t storedType = NVS_TYPE_ANY;
    const esp_err_t findResult = nvs_find_key(m_valueHandle, key, &storedType);
    if (findResult == ESP_ERR_NVS_NOT_FOUND) return EnNvsResult::NotFound;
    if (findResult != ESP_OK) return EnNvsResult::RemoveFailed;

    const esp_err_t valueEraseResult = nvs_erase_key(m_valueHandle, key);
    const esp_err_t metadataEraseResult = nvs_erase_key(m_metadataHandle, key);
    if (valueEraseResult != ESP_OK ||
        (metadataEraseResult != ESP_OK &&
         metadataEraseResult != ESP_ERR_NVS_NOT_FOUND))
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::RemoveFailed;
    }

    if (nvs_commit(m_valueHandle) != ESP_OK ||
        nvs_commit(m_metadataHandle) != ESP_OK)
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::RemoveFailed;
    }
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::Clear()
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    if (nvs_erase_all(m_valueHandle) != ESP_OK ||
        nvs_erase_all(m_metadataHandle) != ESP_OK)
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::ClearFailed;
    }
    if (nvs_commit(m_valueHandle) != ESP_OK ||
        nvs_commit(m_metadataHandle) != ESP_OK)
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::ClearFailed;
    }
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::List(
    EntryVisitor visitor,
    void* context,
    size_t& itemCount,
    size_t& errorCount) const
{
    const std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_started) return EnNvsResult::NotStarted;
    if (visitor == nullptr) return EnNvsResult::InvalidValue;

    size_t localItemCount = 0;
    size_t localErrorCount = 0;
    nvs_iterator_t iterator = nullptr;
    esp_err_t iteratorResult = nvs_entry_find_in_handle(
        m_valueHandle,
        NVS_TYPE_ANY,
        &iterator);

    while (iteratorResult == ESP_OK)
    {
        nvs_entry_info_t rawInfo{};
        if (nvs_entry_info(iterator, &rawInfo) != ESP_OK)
        {
            nvs_release_iterator(iterator);
            return EnNvsResult::ListFailed;
        }

        NvsEntryInfo entry{};
        strncpy(entry.m_key, rawInfo.key, sizeof(entry.m_key) - 1);
        entry.m_key[sizeof(entry.m_key) - 1] = '\0';
        entry.m_result = GetMetadataType(entry.m_key, entry.m_type);
        if (entry.m_result == EnNvsResult::Ok)
        {
            entry.m_result = ValidateStoredValue(entry.m_key, entry.m_type);
        }

        if (entry.m_result == EnNvsResult::Ok) ++localItemCount;
        else ++localErrorCount;
        visitor(entry, context);
        iteratorResult = nvs_entry_next(&iterator);
    }

    nvs_release_iterator(iterator);
    if (iteratorResult != ESP_ERR_NVS_NOT_FOUND)
    {
        return EnNvsResult::ListFailed;
    }

    itemCount = localItemCount;
    errorCount = localErrorCount;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::ValidateRead(
    const char* key,
    EnNvsValueType expectedType) const
{
    if (!m_started) return EnNvsResult::NotStarted;
    const EnNvsResult keyResult = ValidateKey(key);
    if (keyResult != EnNvsResult::Ok) return keyResult;

    nvs_type_t storedNvsType = NVS_TYPE_ANY;
    const esp_err_t findResult = nvs_find_key(
        m_valueHandle,
        key,
        &storedNvsType);
    if (findResult == ESP_ERR_NVS_NOT_FOUND) return EnNvsResult::NotFound;
    if (findResult != ESP_OK) return EnNvsResult::ReadFailed;

    EnNvsValueType storedLogicalType = EnNvsValueType::Unknown;
    const EnNvsResult metadataResult = GetMetadataType(
        key,
        storedLogicalType);
    if (metadataResult != EnNvsResult::Ok) return metadataResult;
    if (storedLogicalType != expectedType) return EnNvsResult::TypeMismatch;

    nvs_type_t expectedNvsType = NVS_TYPE_ANY;
    switch (expectedType)
    {
    case EnNvsValueType::Bool:
    case EnNvsValueType::U8: expectedNvsType = NVS_TYPE_U8; break;
    case EnNvsValueType::I8: expectedNvsType = NVS_TYPE_I8; break;
    case EnNvsValueType::I16: expectedNvsType = NVS_TYPE_I16; break;
    case EnNvsValueType::U16: expectedNvsType = NVS_TYPE_U16; break;
    case EnNvsValueType::I32: expectedNvsType = NVS_TYPE_I32; break;
    case EnNvsValueType::U32: expectedNvsType = NVS_TYPE_U32; break;
    case EnNvsValueType::I64: expectedNvsType = NVS_TYPE_I64; break;
    case EnNvsValueType::U64: expectedNvsType = NVS_TYPE_U64; break;
    case EnNvsValueType::Float:
    case EnNvsValueType::Double: expectedNvsType = NVS_TYPE_BLOB; break;
    case EnNvsValueType::String: expectedNvsType = NVS_TYPE_STR; break;
    default: return EnNvsResult::InvalidType;
    }
    return storedNvsType == expectedNvsType
        ? EnNvsResult::Ok
        : EnNvsResult::TypeMismatch;
}

EnNvsResult NvsPreferenceStore::ValidateStoredValue(
    const char* key,
    EnNvsValueType type) const
{
    nvs_type_t storedType = NVS_TYPE_ANY;
    const esp_err_t findResult = nvs_find_key(m_valueHandle, key, &storedType);
    if (findResult == ESP_ERR_NVS_NOT_FOUND) return EnNvsResult::NotFound;
    if (findResult != ESP_OK) return EnNvsResult::ReadFailed;

    nvs_type_t expectedType = NVS_TYPE_ANY;
    switch (type)
    {
    case EnNvsValueType::Bool:
    case EnNvsValueType::U8: expectedType = NVS_TYPE_U8; break;
    case EnNvsValueType::I8: expectedType = NVS_TYPE_I8; break;
    case EnNvsValueType::I16: expectedType = NVS_TYPE_I16; break;
    case EnNvsValueType::U16: expectedType = NVS_TYPE_U16; break;
    case EnNvsValueType::I32: expectedType = NVS_TYPE_I32; break;
    case EnNvsValueType::U32: expectedType = NVS_TYPE_U32; break;
    case EnNvsValueType::I64: expectedType = NVS_TYPE_I64; break;
    case EnNvsValueType::U64: expectedType = NVS_TYPE_U64; break;
    case EnNvsValueType::Float:
    case EnNvsValueType::Double: expectedType = NVS_TYPE_BLOB; break;
    case EnNvsValueType::String: expectedType = NVS_TYPE_STR; break;
    default: return EnNvsResult::InvalidType;
    }
    if (storedType != expectedType) return EnNvsResult::TypeMismatch;

    if (type == EnNvsValueType::Bool)
    {
        uint8_t value = 0;
        const EnNvsResult readResult = ConvertReadResult(
            nvs_get_u8(m_valueHandle, key, &value));
        if (readResult != EnNvsResult::Ok) return readResult;
        if (value > 1) return EnNvsResult::InvalidBoolean;
    }
    else if (type == EnNvsValueType::Float ||
             type == EnNvsValueType::Double)
    {
        size_t length = 0;
        const EnNvsResult readResult = ConvertReadResult(
            nvs_get_blob(m_valueHandle, key, nullptr, &length));
        if (readResult != EnNvsResult::Ok) return readResult;
        const size_t expectedLength = type == EnNvsValueType::Float
            ? sizeof(float)
            : sizeof(double);
        if (length != expectedLength) return EnNvsResult::TypeMismatch;
    }
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::GetMetadataType(
    const char* key,
    EnNvsValueType& type) const
{
    uint8_t rawType = 0;
    const esp_err_t readResult = nvs_get_u8(
        m_metadataHandle,
        key,
        &rawType);
    if (readResult == ESP_ERR_NVS_NOT_FOUND)
    {
        return EnNvsResult::TypeMetadataNotFound;
    }
    if (readResult != ESP_OK)
    {
        return EnNvsResult::ReadFailed;
    }
    if (rawType < static_cast<uint8_t>(EnNvsValueType::Bool) ||
        rawType > static_cast<uint8_t>(EnNvsValueType::String))
    {
        return EnNvsResult::TypeMetadataNotFound;
    }

    type = static_cast<EnNvsValueType>(rawType);
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::FinishSet(
    const char* key,
    EnNvsValueType type,
    esp_err_t valueSetResult)
{
    if (valueSetResult != ESP_OK)
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::SaveFailed;
    }

    if (nvs_set_u8(
            m_metadataHandle,
            key,
            static_cast<uint8_t>(type)) != ESP_OK)
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::MetadataSaveFailed;
    }

    if (nvs_commit(m_valueHandle) != ESP_OK)
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::SaveFailed;
    }
    if (nvs_commit(m_metadataHandle) != ESP_OK)
    {
        ReopenAfterWriteFailure();
        return EnNvsResult::MetadataSaveFailed;
    }
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::ReopenAfterWriteFailure()
{
    nvs_close(m_metadataHandle);
    nvs_close(m_valueHandle);
    m_metadataHandle = 0;
    m_valueHandle = 0;
    m_started = false;

    if (nvs_open(
            m_namespace.c_str(),
            NVS_READWRITE,
            &m_valueHandle) != ESP_OK)
    {
        m_valueHandle = 0;
        return EnNvsResult::SaveFailed;
    }
    if (nvs_open(
            m_metadataNamespace.c_str(),
            NVS_READWRITE,
            &m_metadataHandle) != ESP_OK)
    {
        nvs_close(m_valueHandle);
        m_valueHandle = 0;
        m_metadataHandle = 0;
        return EnNvsResult::SaveFailed;
    }

    m_started = true;
    return EnNvsResult::Ok;
}

EnNvsResult NvsPreferenceStore::ConvertReadResult(esp_err_t result)
{
    if (result == ESP_OK) return EnNvsResult::Ok;
    if (result == ESP_ERR_NVS_TYPE_MISMATCH)
    {
        return EnNvsResult::TypeMismatch;
    }
    return EnNvsResult::ReadFailed;
}
