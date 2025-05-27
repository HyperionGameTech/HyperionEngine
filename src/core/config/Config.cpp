/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/config/Config.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypDataJSONHelpers.hpp>

#include <core/algorithm/FindIf.hpp>

#include <core/utilities/Format.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {
namespace config {

static const ConfigurationValue g_invalid_configuration_value {};

#pragma region ConfigurationDataStore

ConfigurationDataStore::ConfigurationDataStore(UTF8StringView config_name)
    : DataStoreBase("config", DataStoreOptions { /* flags */ DSF_RW, /* max_size */ 0ull }),
      m_config_name(config_name)
{
}

ConfigurationDataStore::ConfigurationDataStore(ConfigurationDataStore&& other) noexcept
    : DataStoreBase(static_cast<DataStoreBase&&>(std::move(other))),
      m_config_name(std::move(other.m_config_name))
{
}

ConfigurationDataStore::~ConfigurationDataStore()
{
}

FilePath ConfigurationDataStore::GetFilePath() const
{
    FilePath config_path = GetDirectory() / m_config_name;

    if (!config_path.EndsWith(".json"))
    {
        config_path = config_path + ".json";
    }

    return config_path;
}

bool ConfigurationDataStore::Read(json::JSONValue& out_value) const
{
    const FilePath config_path = GetFilePath();

    if (!config_path.Exists())
    {
        return false;
    }

    BufferedReader reader;

    if (!config_path.Open(reader))
    {
        return false;
    }

    json::ParseResult parse_result = json::JSON::Parse(String(reader.ReadBytes().ToByteView()));

    if (!parse_result.ok)
    {
        HYP_LOG(Config, Warning, "Invalid JSON in configuration file at {}: {}", config_path, parse_result.message);

        return false;
    }

    out_value = std::move(parse_result.value);

    return true;
}

bool ConfigurationDataStore::Write(const json::JSONValue& value) const
{
    const String value_string = value.ToString(true);

    FileByteWriter writer(GetFilePath());
    writer.WriteString(value_string, BYTE_WRITER_FLAGS_NONE);
    writer.Close();

    return true;
}

#pragma endregion ConfigurationDataStore

#pragma region ConfigurationTable

ConfigurationTable::ConfigurationTable()
    : m_root_object(json::JSONObject()),
      m_data_store(nullptr)
{
}

ConfigurationTable::ConfigurationTable(const String& config_name, const String& subobject_path)
    : m_subobject_path(subobject_path.Any() ? subobject_path : Optional<String> {}),
      m_root_object(json::JSONObject()),
      m_data_store(&DataStoreBase::GetOrCreate<ConfigurationDataStore>(config_name)),
      m_data_store_resource_handle(*m_data_store)
{
    // try to read from config file
    if (!m_data_store->Read(m_root_object))
    {
        HYP_LOG(Config, Warning, "Configuration could not be read: {}", m_data_store->GetFilePath());
    }

    m_cached_hash_code = GetSubobject().GetHashCode();
}

ConfigurationTable::ConfigurationTable(const String& config_name)
    : ConfigurationTable(config_name, String::empty)
{
}

ConfigurationTable::ConfigurationTable(const String& config_name, const HypClass* hyp_class)
    : ConfigurationTable(config_name, hyp_class ? hyp_class->GetAttribute("jsonpath").GetString() : String::empty)
{
}

ConfigurationTable::ConfigurationTable(const ConfigurationTable& other)
    : m_subobject_path(other.m_subobject_path),
      m_root_object(other.m_root_object),
      m_data_store(other.m_data_store),
      m_data_store_resource_handle(other.m_data_store_resource_handle),
      m_cached_hash_code(other.m_cached_hash_code)
{
}

ConfigurationTable& ConfigurationTable::operator=(const ConfigurationTable& other)
{
    if (this == &other)
    {
        return *this;
    }

    m_subobject_path = other.m_subobject_path;
    m_root_object = other.m_root_object;
    m_data_store = other.m_data_store;
    m_data_store_resource_handle = other.m_data_store_resource_handle;
    m_cached_hash_code = other.m_cached_hash_code;

    return *this;
}

ConfigurationTable::ConfigurationTable(ConfigurationTable&& other) noexcept
    : m_subobject_path(std::move(other.m_subobject_path)),
      m_root_object(std::move(other.m_root_object)),
      m_data_store(other.m_data_store),
      m_data_store_resource_handle(std::move(other.m_data_store_resource_handle)),
      m_cached_hash_code(std::move(other.m_cached_hash_code))
{
    other.m_data_store = nullptr;
}

ConfigurationTable& ConfigurationTable::operator=(ConfigurationTable&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_subobject_path = std::move(other.m_subobject_path);
    m_root_object = std::move(other.m_root_object);
    m_data_store = other.m_data_store;
    m_data_store_resource_handle = std::move(other.m_data_store_resource_handle);
    m_cached_hash_code = std::move(other.m_cached_hash_code);

    other.m_data_store = nullptr;

    return *this;
}

bool ConfigurationTable::IsChanged() const
{
    return GetSubobject().GetHashCode() != m_cached_hash_code;
}

ConfigurationTable& ConfigurationTable::Merge(const ConfigurationTable& other)
{
    if (this == &other)
    {
        return *this;
    }

    const json::JSONValue& other_subobject = other.GetSubobject();

    if (!other_subobject.IsObject())
    {
        return *this;
    }

    json::JSONValue& target_object = other.m_subobject_path.HasValue()
        ? *m_root_object.Get(*other.m_subobject_path, /* create_intermediate_objects */ true)
        : m_root_object;

    if (!target_object.IsObject())
    {
        target_object = json::JSONObject();
    }

    target_object.AsObject().Merge(other_subobject.AsObject());

    return *this;
}

const ConfigurationValue& ConfigurationTable::Get(UTF8StringView key) const
{
    auto select_result = GetSubobject().Get(key);

    if (select_result.value != nullptr)
    {
        return *select_result.value;
    }

    return g_invalid_configuration_value;
}

void ConfigurationTable::Set(UTF8StringView key, const ConfigurationValue& value)
{
    GetSubobject().Set(key, value);
}

bool ConfigurationTable::Save()
{
    AssertThrow(m_data_store != nullptr);

    if (m_data_store->Write(m_root_object))
    {
        m_cached_hash_code = GetSubobject().GetHashCode();

        return true;
    }

    return false;
}

json::JSONValue& ConfigurationTable::GetSubobject()
{
    json::JSONValue* subobject = &m_root_object;

    if (m_subobject_path.HasValue())
    {
        subobject = &*m_root_object.Get(*m_subobject_path, /* create_intermediate_objects */ true);

        if (!subobject->IsObject())
        {
            *subobject = json::JSONObject();
        }
    }

    return *subobject;
}

const json::JSONValue& ConfigurationTable::GetSubobject() const
{
    const json::JSONValue* subobject = &m_root_object;

    if (m_subobject_path.HasValue())
    {
        subobject = &*m_root_object.Get(*m_subobject_path);

        if (!subobject->IsObject())
        {
            subobject = &json::JSONValue::s_empty_object;
        }
    }

    return *subobject;
}

const String& ConfigurationTable::GetDefaultConfigName(const HypClass* hyp_class)
{
    if (hyp_class)
    {
        if (const HypClassAttributeValue& config_name_attribute_value = hyp_class->GetAttribute("configname"))
        {
            return config_name_attribute_value.GetString();
        }
    }

    return String::empty;
}

void ConfigurationTable::AddError(const Error& error)
{
    m_errors.PushBack(error);
}

void ConfigurationTable::LogErrors() const
{
    if (m_errors.Empty())
    {
        return;
    }

    HYP_LOG(Config, Error, "Errors in configuration \"{}\" ({}):",
        m_data_store ? m_data_store->GetConfigName() : "<unknown>",
        m_data_store ? m_data_store->GetFilePath() : "<unknown>");

    for (const Error& error : m_errors)
    {
        HYP_LOG(Config, Error, "  <{}> {}", error.GetFunctionName(), error.GetMessage());
    }
}

void ConfigurationTable::LogErrors(UTF8StringView message) const
{
    HYP_LOG(Config, Error, "Errors in configuration \"{}\" ({}):",
        m_data_store ? m_data_store->GetConfigName() : "<unknown>",
        m_data_store ? m_data_store->GetFilePath() : "<unknown>");

    for (const Error& error : m_errors)
    {
        HYP_LOG(Config, Error, "  <{}> {}", error.GetFunctionName(), error.GetMessage());
    }

    HYP_LOG(Config, Error, "{}", message);
}

bool ConfigurationTable::SetHypClassFields(const HypClass* hyp_class, const void* ptr)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(ptr != nullptr);

    AnyRef target_ref(hyp_class->GetTypeID(), const_cast<void*>(ptr));
    HypData target_hyp_data = HypData(target_ref);

    if (!JSONToObject(GetSubobject().AsObject(), hyp_class, target_hyp_data))
    {
        HYP_LOG(Config, Error, "Failed to deserialize JSON to instance of HypClass \"{}\"", hyp_class->GetName());

        return false;
    }

    json::JSONObject json_object;

    if (ObjectToJSON(hyp_class, target_hyp_data, json_object))
    {
        GetSubobject().AsObject() = std::move(json_object.Merge(GetSubobject().AsObject()));

        return true;
    }
    else
    {
        HYP_LOG(Config, Error, "Failed to serialize HypClass \"{}\" to json", hyp_class->GetName());

        return false;
    }
}

#pragma endregion ConfigurationTable

} // namespace config
} // namespace hyperion