/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/config/Config.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypDataJSONHelpers.hpp>

#include <core/utilities/Format.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {
namespace config {

static const ConfigurationValue g_invalidConfigurationValue {};

#pragma region ConfigurationTable

ConfigurationTable::ConfigurationTable()
    : m_rootObject(json::JSONObject())
{
}

ConfigurationTable::ConfigurationTable(const String& configName, const String& subobjectPath)
    : m_subobjectPath(subobjectPath.Any() ? subobjectPath : Optional<String> {}),
      m_rootObject(json::JSONObject()),
      m_name(configName)
{
    // try to read from config file
    if (auto result = Read(m_rootObject); result.HasError())
    {
        HYP_LOG(Config, Error, "Failed to read configuration file at {}: {}", GetFilePath(), result.GetError().GetMessage());

        return;
    }

    m_cachedHashCode = GetSubobject().GetHashCode();
}

ConfigurationTable::ConfigurationTable(const String& configName)
    : ConfigurationTable(configName, String::empty)
{
}

ConfigurationTable::ConfigurationTable(const String& configName, const HypClass* hypClass)
    : ConfigurationTable(configName, hypClass ? hypClass->GetAttribute("jsonpath").GetString() : String::empty)
{
}

ConfigurationTable::ConfigurationTable(const ConfigurationTable& other)
    : m_subobjectPath(other.m_subobjectPath),
      m_rootObject(other.m_rootObject),
      m_name(other.m_name),
      m_cachedHashCode(other.m_cachedHashCode)
{
}

ConfigurationTable& ConfigurationTable::operator=(const ConfigurationTable& other)
{
    if (this == &other)
    {
        return *this;
    }

    m_subobjectPath = other.m_subobjectPath;
    m_rootObject = other.m_rootObject;
    m_name = other.m_name;
    m_cachedHashCode = other.m_cachedHashCode;

    return *this;
}

ConfigurationTable::ConfigurationTable(ConfigurationTable&& other) noexcept
    : m_subobjectPath(std::move(other.m_subobjectPath)),
      m_rootObject(std::move(other.m_rootObject)),
      m_name(std::move(other.m_name)),
      m_cachedHashCode(std::move(other.m_cachedHashCode))
{
}

ConfigurationTable& ConfigurationTable::operator=(ConfigurationTable&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_subobjectPath = std::move(other.m_subobjectPath);
    m_rootObject = std::move(other.m_rootObject);
    m_name = std::move(other.m_name);
    m_cachedHashCode = std::move(other.m_cachedHashCode);

    return *this;
}

bool ConfigurationTable::IsChanged() const
{
    return GetSubobject().GetHashCode() != m_cachedHashCode;
}

FilePath ConfigurationTable::GetFilePath() const
{
    FilePath configPath = GetResourceDirectory() / "config" / m_name;

    if (!configPath.EndsWith(".json"))
    {
        configPath = configPath + ".json";
    }

    return configPath;
}

Result ConfigurationTable::Read(json::JSONValue& outValue) const
{
    const FilePath configPath = GetFilePath();

    if (!configPath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Configuration file does not exist at {}", configPath);
    }

    FileBufferedReaderSource source { configPath };
    BufferedReader reader { &source };

    if (!reader.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open configuration file at {}", configPath);
    }

    json::ParseResult parseResult = json::JSON::Parse(String(reader.ReadBytes().ToByteView()));

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse configuration file at {}: {}", configPath, parseResult.message);
    }

    outValue = std::move(parseResult.value);

    return {};
}

Result ConfigurationTable::Write(const json::JSONValue& value) const
{
    const String valueString = value.ToString(true);

    FileByteWriter writer { GetFilePath() };
    writer.WriteString(valueString, BYTE_WRITER_FLAGS_NONE);
    writer.Close();

    return {};
}

ConfigurationTable& ConfigurationTable::Merge(const ConfigurationTable& other)
{
    if (this == &other)
    {
        return *this;
    }

    const json::JSONValue& otherSubobject = other.GetSubobject();

    if (!otherSubobject.IsObject())
    {
        return *this;
    }

    json::JSONValue& targetObject = other.m_subobjectPath.HasValue()
        ? *m_rootObject.Get(*other.m_subobjectPath, /* createIntermediateObjects */ true)
        : m_rootObject;

    if (!targetObject.IsObject())
    {
        targetObject = json::JSONObject();
    }

    targetObject.AsObject().MergeDeep(otherSubobject.AsObject());

    return *this;
}

const ConfigurationValue& ConfigurationTable::Get(UTF8StringView key) const
{
    auto selectResult = GetSubobject().Get(key);

    if (selectResult.value != nullptr)
    {
        return *selectResult.value;
    }

    return g_invalidConfigurationValue;
}

void ConfigurationTable::Set(UTF8StringView key, const ConfigurationValue& value)
{
    GetSubobject().Set(key, value);
}

bool ConfigurationTable::Save()
{
    if (auto result = Write(m_rootObject); result.HasError())
    {
        HYP_LOG(Config, Error, "Failed to write configuration file at {}: {}", GetFilePath(), result.GetError().GetMessage());

        return false;
    }

    m_cachedHashCode = GetSubobject().GetHashCode();

    return true;
}

json::JSONValue& ConfigurationTable::GetSubobject()
{
    json::JSONValue* subobject = &m_rootObject;

    if (m_subobjectPath.HasValue())
    {
        subobject = &*m_rootObject.Get(*m_subobjectPath, /* createIntermediateObjects */ true);

        if (!subobject->IsObject())
        {
            *subobject = json::JSONObject();
        }
    }

    return *subobject;
}

const json::JSONValue& ConfigurationTable::GetSubobject() const
{
    const json::JSONValue* subobject = &m_rootObject;

    if (m_subobjectPath.HasValue())
    {
        subobject = &*m_rootObject.Get(*m_subobjectPath);

        if (!subobject->IsObject())
        {
            subobject = &json::JSON::EmptyObject();
        }
    }

    return *subobject;
}

const String& ConfigurationTable::GetDefaultConfigName(const HypClass* hypClass)
{
    if (hypClass)
    {
        if (const HypClassAttributeValue& configNameAttributeValue = hypClass->GetAttribute("configname"))
        {
            return configNameAttributeValue.GetString();
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

    HYP_LOG(Config, Error, "Errors in configuration \"{}\" ({}):", m_name, GetFilePath());

    for (const Error& error : m_errors)
    {
        HYP_LOG(Config, Error, "  <{}> {}", error.GetFunctionName(), error.GetMessage());
    }
}

void ConfigurationTable::LogErrors(UTF8StringView message) const
{
    HYP_LOG(Config, Error, "Errors in configuration \"{}\" ({}):", m_name, GetFilePath());

    for (const Error& error : m_errors)
    {
        HYP_LOG(Config, Error, "  <{}> {}", error.GetFunctionName(), error.GetMessage());
    }

    HYP_LOG(Config, Error, "{}", message);
}

bool ConfigurationTable::SetHypClassFields(const HypClass* hypClass, const void* ptr)
{
    HYP_CORE_ASSERT(hypClass != nullptr);
    HYP_CORE_ASSERT(ptr != nullptr);

    AnyRef targetRef(hypClass->GetTypeId(), const_cast<void*>(ptr));
    HypData targetHypData = HypData(targetRef);

    if (!JSONToObject(GetSubobject().AsObject(), hypClass, targetHypData))
    {
        HYP_LOG(Config, Error, "Failed to deserialize JSON to instance of HypClass \"{}\"", hypClass->GetName());

        return false;
    }

    json::JSONObject jsonObject;

    if (ObjectToJSON(hypClass, targetHypData, jsonObject))
    {
        jsonObject.Merge(GetSubobject().AsObject());

        GetSubobject().AsObject() = std::move(jsonObject);

        return true;
    }
    else
    {
        HYP_LOG(Config, Error, "Failed to serialize HypClass \"{}\" to json", hypClass->GetName());

        return false;
    }
}

#pragma endregion ConfigurationTable

} // namespace config
} // namespace hyperion
