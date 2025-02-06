/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/config/Config.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <system/AppContext.hpp>

#include <asset/ByteWriter.hpp>
#include <asset/BufferedByteReader.hpp>

#include <Engine.hpp>

namespace hyperion {
namespace config {

static const ConfigurationValue g_invalid_configuration_value { };

#pragma region ConfigurationDataStore

ConfigurationDataStore::ConfigurationDataStore(UTF8StringView config_name)
    : DataStoreBase("config", DataStoreOptions { /* flags */ DSF_RW, /* max_size */ 0ull }),
      m_config_name(config_name)
{
}

ConfigurationDataStore::ConfigurationDataStore(ConfigurationDataStore &&other) noexcept
    : DataStoreBase(static_cast<DataStoreBase &&>(std::move(other))),
      m_config_name(std::move(other.m_config_name))
{
}

ConfigurationDataStore::~ConfigurationDataStore()
{
}

FilePath ConfigurationDataStore::GetFilePath() const
{
    FilePath config_path = GetDirectory() / m_config_name;

    if (!config_path.EndsWith(".json")) {
        config_path = config_path + ".json";
    }

    return config_path;
}

bool ConfigurationDataStore::Read(json::JSONValue &out_value) const
{
    const FilePath config_path = GetFilePath();
    
    if (!config_path.Exists()) {
        return false;
    }

    BufferedReader reader;

    if (!config_path.Open(reader)) {
        return false;
    }

    json::ParseResult parse_result = json::JSON::Parse(String(reader.ReadBytes().ToByteView()));

    if (!parse_result.ok) {
        HYP_LOG(Config, Warning, "Invalid JSON in configuration file at {}: {}", config_path, parse_result.message);

        return false;
    }

    out_value = std::move(parse_result.value);

    return true;
}

bool ConfigurationDataStore::Write(const json::JSONValue &value) const
{
    const String value_string = value.ToString(true);

    FileByteWriter writer(GetFilePath());
    writer.WriteString(value_string, BYTE_WRITER_FLAGS_NONE);
    writer.Close();
    
    return true;
}

#pragma endregion ConfigurationDataStore

#pragma region ConfigurationTable

ConfigurationTable::ConfigurationTable(const String &config_name)
    : m_data_store(&DataStoreBase::GetOrCreate<ConfigurationDataStore>(config_name)),
      m_data_store_resource_handle(*m_data_store),
      m_root_object(json::JSONObject()),
      m_cached_hash_code(m_root_object.GetHashCode())
{
    HYP_LOG(Config, Debug, "Reading from configuration {}", config_name);

    // try to read from config file
    if (m_data_store->Read(m_root_object)) {
        m_cached_hash_code = m_root_object.GetHashCode();
    } else {
        HYP_LOG(Config, Info, "Configuration could not be read: {}", m_data_store->GetFilePath());
    }
}

ConfigurationTable::ConfigurationTable(const ConfigurationTable &other)
    : m_data_store(other.m_data_store),
      m_data_store_resource_handle(other.m_data_store_resource_handle),
      m_root_object(other.m_root_object),
      m_cached_hash_code(other.m_cached_hash_code)
{
}

ConfigurationTable &ConfigurationTable::operator=(const ConfigurationTable &other)
{
    if (this == &other) {
        return *this;
    }

    m_data_store = other.m_data_store;
    m_data_store_resource_handle = other.m_data_store_resource_handle;
    m_root_object = other.m_root_object;
    m_cached_hash_code = other.m_cached_hash_code;

    return *this;
}

ConfigurationTable::ConfigurationTable(ConfigurationTable &&other) noexcept
    : m_data_store(std::move(other.m_data_store)),
      m_data_store_resource_handle(std::move(other.m_data_store_resource_handle)),
      m_root_object(std::move(other.m_root_object)),
      m_cached_hash_code(std::move(other.m_cached_hash_code))
{
}

ConfigurationTable &ConfigurationTable::operator=(ConfigurationTable &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_data_store = std::move(other.m_data_store);
    m_data_store_resource_handle = std::move(other.m_data_store_resource_handle);
    m_root_object = std::move(other.m_root_object);
    m_cached_hash_code = std::move(other.m_cached_hash_code);

    other.m_data_store = nullptr;

    return *this;
}

bool ConfigurationTable::IsChanged() const
{
    return m_root_object.GetHashCode() != m_cached_hash_code;
}

ConfigurationTable &ConfigurationTable::Merge(const ConfigurationTable &other)
{
    if (!m_root_object.IsObject() || !other.m_root_object.IsObject()) {
        return *this;
    }

    m_root_object.AsObject().Merge(other.m_root_object.AsObject());

    return *this;
}

const ConfigurationValue &ConfigurationTable::Get(UTF8StringView key) const
{
    auto select_result = m_root_object.Get(key);

    if (select_result.value != nullptr) {
        return *select_result.value;
    }

    return g_invalid_configuration_value;
}

void ConfigurationTable::Set(UTF8StringView key, const ConfigurationValue &value)
{
    m_root_object.Set(key, value);
}

bool ConfigurationTable::Save() const
{
    AssertThrow(m_data_store != nullptr);

    if (m_data_store->Write(m_root_object)) {
        m_cached_hash_code = m_root_object.GetHashCode();

        return true;
    }

    return false;
}

#pragma endregion ConfigurationTable

HYP_API const ConfigurationTable &GetGlobalConfigurationTable()
{
    AssertThrow(g_engine.IsValid());
    AssertThrow(g_engine->GetAppContext() != nullptr);

    return g_engine->GetAppContext()->GetConfiguration();
}

} // namespace config
} // namespace hyperion