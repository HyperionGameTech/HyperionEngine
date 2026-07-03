/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Config/Config.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/StaticField.hpp>

#include <Core/Utilities/Format.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

namespace Hyperion {

namespace CoreApi {
CORE_API extern FilePath GetConfigDirectory();
} // namespace CoreApi

namespace config {

static const ConfigValue s_invalidConfigValue {};

static Map<String, JSON::Value> s_configCache;
static SharedMutex s_configCacheMutex;

#pragma region ConfigBase

// Set externally for DI
CORE_API Result(*ConfigBase::s_ObjectFromJSON)(const JSON::Object& jsonObject, const Class* targetClass, BoxedValue& target) = nullptr;
CORE_API Result(*ConfigBase::s_ObjectToJSON)(const Class* cls, const BoxedValue& target, JSON::Object& outJson, struct ToJSONOptions* pOptions) = nullptr;

ConfigBase::ConfigBase()
    : m_rootObject(JSON::Object())
{
}

ConfigBase::ConfigBase(const String& configName, const String& subobjectPath)
    : m_subobjectPath(subobjectPath.Any() ? subobjectPath : Optional<String> {}),
      m_rootObject(JSON::Object()),
      m_name(configName)
{
}

ConfigBase::ConfigBase(const String& configName)
    : ConfigBase(configName, String::empty)
{
}

ConfigBase::ConfigBase(const String& configName, const Class* cls)
    : ConfigBase(configName, cls ? cls->GetAttribute(Attributes::g_attrJsonPath).GetString() : UTF8StringView())
{
}

ConfigBase::ConfigBase(const ConfigBase& other)
    : m_subobjectPath(other.m_subobjectPath),
      m_rootObject(other.m_rootObject),
      m_name(other.m_name),
      m_cachedHashCode(other.m_cachedHashCode)
{
}

ConfigBase& ConfigBase::operator=(const ConfigBase& other)
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

ConfigBase::ConfigBase(ConfigBase&& other) noexcept
    : m_subobjectPath(std::move(other.m_subobjectPath)),
      m_rootObject(std::move(other.m_rootObject)),
      m_name(std::move(other.m_name)),
      m_cachedHashCode(std::move(other.m_cachedHashCode))
{
}

ConfigBase& ConfigBase::operator=(ConfigBase&& other) noexcept
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

bool ConfigBase::IsChanged() const
{
    return GetSubobject().GetHashCode() != m_cachedHashCode;
}

FilePath ConfigBase::GetFilePath() const
{
    FilePath configPath = CoreApi::GetConfigDirectory() / m_name;

    if (!configPath.EndsWith(".json"))
    {
        configPath = configPath + ".json";
    }

    return configPath;
}

FilePath ConfigBase::GetPlatformFilePath() const
{
#if defined(HYP_PLATFORM_NAME_STR)
    String name = m_name;

    if (name.EndsWith(".json"))
    {
        name = name.Substr(0, name.Length() - 5);
    }

    return CoreApi::GetConfigDirectory() / (name + "." HYP_PLATFORM_NAME_STR ".json");
#else
    return GetFilePath();
#endif
}

Result ConfigBase::Read(JSON::Value& outValue) const
{
    return Read(GetFilePath(), outValue);
}

Result ConfigBase::Read(const FilePath& configPath, JSON::Value& outValue) const
{
    if (!configPath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Configuration file does not exist at {}", configPath);
    }

    FileByteReader stream { configPath };

    if (stream.Eof())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open configuration file at {}", configPath);
    }

    ByteBuffer buffer = stream.Read();
    String configStr = String(buffer.ToByteView());

    JSON::ParseResult parseResult = JSON::Parse(configStr);

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse configuration file at {}: {}", configPath, parseResult.message);
    }

    outValue = std::move(parseResult.value);

    return {};
}

Result ConfigBase::Write(const JSON::Value& value) const
{
    const String valueString = value.ToString(true);

    FileByteWriter writer { GetFilePath() };
    writer.WriteString(valueString, BYTE_WRITER_FLAGS_NONE);
    writer.Close();

    return {};
}

ConfigBase& ConfigBase::Merge(const ConfigBase& other)
{
    if (this == &other)
    {
        return *this;
    }

    const JSON::Value& otherSubobject = other.GetSubobject();

    if (!otherSubobject.IsObject())
    {
        return *this;
    }

    JSON::Value& targetObject = other.m_subobjectPath.HasValue()
        ? *m_rootObject.Get(*other.m_subobjectPath, /* createIntermediateObjects */ true)
        : m_rootObject;

    if (!targetObject.IsObject())
    {
        targetObject = JSON::Object();
    }

    targetObject.AsObject().MergeDeep(otherSubobject.AsObject());

    return *this;
}

const ConfigValue& ConfigBase::Get(UTF8StringView key) const
{
    auto selectResult = GetSubobject().Get(key);

    if (selectResult.value != nullptr)
    {
        return *selectResult.value;
    }

    return s_invalidConfigValue;
}

void ConfigBase::Set(UTF8StringView key, const ConfigValue& value)
{
    GetSubobject().Set(key, value);
}

bool ConfigBase::Save()
{
    // Update in-memory cache
    {
        TUniqueLock lock(s_configCacheMutex);
        s_configCache.Set(m_name, m_rootObject);
    }

#if !defined(HYP_ANDROID) && !defined(HYP_IOS)
    // Write to file
    if (auto result = Write(m_rootObject); result.HasError())
    {
        HYP_LOG(Config, Warning, "Failed to write configuration file at {}: {}", GetFilePath(), result.GetError().GetMessage());
    }
#endif

    m_cachedHashCode = GetSubobject().GetHashCode();

    return true;
}

bool ConfigBase::Load()
{
    // Check cache
    {
        TSharedLock lock(s_configCacheMutex);
        if (auto it = s_configCache.Find(m_name); it != s_configCache.End())
        {
            m_rootObject = it->second;
            m_cachedHashCode = GetSubobject().GetHashCode();
            return true;
        }
    }

    // Cache miss, read from file.

    const FilePath platformPath = GetPlatformFilePath();

    if (Result result = Read(platformPath, m_rootObject); result.HasError())
    {
        // Fall back to default
        if (Result fallbackResult = Read(m_rootObject); fallbackResult.HasError())
        {
            m_errors.PushBack(fallbackResult.GetError());
            return false;
        }
    }

    // Store in cache
    {
        TUniqueLock lock(s_configCacheMutex);
        s_configCache.Set(m_name, m_rootObject);
    }

    m_cachedHashCode = GetSubobject().GetHashCode();

    return true;
}

JSON::Value& ConfigBase::GetSubobject()
{
    JSON::Value* subobject = &m_rootObject;

    if (m_subobjectPath.HasValue())
    {
        subobject = &*m_rootObject.Get(*m_subobjectPath, /* createIntermediateObjects */ true);

        if (!subobject->IsObject())
        {
            *subobject = JSON::Object();
        }
    }

    return *subobject;
}

const JSON::Value& ConfigBase::GetSubobject() const
{
    const JSON::Value* subobject = &m_rootObject;

    if (m_subobjectPath.HasValue())
    {
        subobject = &*m_rootObject.Get(*m_subobjectPath);

        if (!subobject->IsObject())
        {
            subobject = &JSON::EmptyObject();
        }
    }

    return *subobject;
}

UTF8StringView ConfigBase::GetDefaultConfigName(const Class* cls)
{
    if (cls)
    {
        if (const ClassAttributeValue& configNameAttributeValue = cls->GetAttribute("configname"_sh))
        {
            return configNameAttributeValue.GetString();
        }
    }

    return {};
}

void ConfigBase::AddError(const Error& error)
{
    m_errors.PushBack(error);
}

void ConfigBase::LogErrors(FILE* outFile) const
{
    if (m_errors.Empty())
    {
        return;
    }

    std::fprintf(outFile, "Errors in configuration \"%s\" (%s):", *m_name, *GetFilePath());

    for (const Error& error : m_errors)
    {
        std::fprintf(outFile, "  <%s> %s", error.GetFunctionName(), error.GetMessage());
    }
}

void ConfigBase::LogErrors(FILE* outFile, UTF8StringView message) const
{
    std::fprintf(outFile, "Errors in configuration \"%s\" (%s):", *m_name, *GetFilePath());

    for (const Error& error : m_errors)
    {
        std::fprintf(outFile, "  <%s> %s", error.GetFunctionName(), error.GetMessage());
    }

    std::fprintf(outFile, "%s", message.Data());
}

bool ConfigBase::SetClassFields(const Class* cls, const void* ptr)
{
    HYP_CORE_ASSERT(cls != nullptr);
    HYP_CORE_ASSERT(ptr != nullptr);

    if (!s_ObjectFromJSON || !s_ObjectToJSON)
    {
        HYP_LOG(Config, Error, "ObjectFromJSON or ObjectToJSON function is not set");
        return false;
    }

    BoxedValue target = BoxedValue(AnyRef(cls->GetTypeInfo(), const_cast<void*>(ptr)));

    if (!s_ObjectFromJSON(GetSubobject().AsObject(), cls, target))
    {
        HYP_LOG(Config, Error, "Failed to deserialize JSON to instance of Class \"{}\"", cls->GetName());

        return false;
    }

    JSON::Object jsonObject;

    if (s_ObjectToJSON(cls, target, jsonObject, nullptr))
    {
        jsonObject.Merge(GetSubobject().AsObject());

        GetSubobject().AsObject() = std::move(jsonObject);

        return true;
    }
    else
    {
        HYP_LOG(Config, Error, "Failed to serialize Class \"{}\" to json", cls->GetName());

        return false;
    }
}

#pragma endregion ConfigBase

} // namespace config
} // namespace Hyperion
