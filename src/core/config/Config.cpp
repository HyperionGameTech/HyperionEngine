/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/config/Config.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <asset/ByteWriter.hpp>
#include <asset/BufferedByteReader.hpp>

namespace hyperion {
namespace config {

static const ConfigurationValue g_invalid_configuration_value { };

#pragma region ConfigurationDataStore

static const FilePath g_config_base_path = FilePath::Join(HYP_ROOT_DIR, "res", "data", "config");

ConfigurationDataStore::ConfigurationDataStore(UTF8StringView config_name)
    : m_config_name(config_name)
{
}

ConfigurationDataStore::~ConfigurationDataStore()
{
}

FilePath ConfigurationDataStore::GetFilePath() const
{
    FilePath config_path = g_config_base_path / m_config_name;

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
        HYP_LOG(Config, LogLevel::WARNING, "Invalid JSON in configuration file at {}: {}", config_path, parse_result.message);

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
    : m_data_store(config_name),
      m_root_object(json::JSONObject()),
      m_cached_hash_code(m_root_object.GetHashCode())
{
    // try to read from config file
    if (m_data_store.Read(m_root_object)) {
        m_cached_hash_code = m_root_object.GetHashCode();
    } else {
        HYP_LOG(Config, LogLevel::INFO, "Configuration could not be read: {}", m_data_store.GetFilePath());
    }
}

bool ConfigurationTable::IsChanged() const
{
    return m_root_object.GetHashCode() != m_cached_hash_code;
}

const ConfigurationValue &ConfigurationTable::Get(UTF8StringView key) const
{
    HYP_MT_CHECK_READ(m_data_race_detector);

    auto select_result = m_root_object.Get(key);

    if (select_result.value != nullptr) {
        return *select_result.value;
    }

    return g_invalid_configuration_value;
}

void ConfigurationTable::Set(UTF8StringView key, const ConfigurationValue &value)
{
    HYP_MT_CHECK_RW(m_data_race_detector);

    m_root_object.Set(key, value);
}

bool ConfigurationTable::Save() const
{
    HYP_MT_CHECK_RW(m_data_race_detector);

    if (m_data_store.Write(m_root_object)) {
        m_cached_hash_code = m_root_object.GetHashCode();

        return true;
    }

    return false;
}

#pragma endregion ConfigurationTable

} // namespace config
} // namespace hyperion