/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/config/Config.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/Property.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/StaticField.hpp>
#include <Core/serialization/SerializationUtils.hpp>

#include <Core/utilities/Format.hpp>
#include <Core/reflection/TypeInfo.hpp>

#include <Core/io/ByteWriter.hpp>
#include <Core/io/ByteReader.hpp>

#include <Core/logging/LogChannels.hpp>
#include <Core/logging/Logger.hpp>

namespace Hyperion {

namespace CoreApi {
extern FilePath GetConfigDirectory();
} // namespace CoreApi

namespace config {

static const ConfigValue s_invalidConfigValue {};

#pragma region ConfigBase

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

Result ConfigBase::Read(JSON::Value& outValue) const
{
    const FilePath configPath = GetFilePath();

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
    if (auto result = Write(m_rootObject); result.HasError())
    {
        HYP_LOG(Config, Error, "Failed to write configuration file at {}: {}", GetFilePath(), result.GetError().GetMessage());

        return false;
    }

    m_cachedHashCode = GetSubobject().GetHashCode();

    return true;
}

bool ConfigBase::Load()
{
    // try to read from config file
    if (Result result = Read(m_rootObject); result.HasError())
    {
        m_errors.PushBack(result.GetError());
        return false;
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

    BoxedValue target = BoxedValue(AnyRef(cls->GetTypeInfo(), const_cast<void*>(ptr)));

    if (!ObjectFromJSON(GetSubobject().AsObject(), cls, target))
    {
        HYP_LOG(Config, Error, "Failed to deserialize JSON to instance of Class \"{}\"", cls->GetName());

        return false;
    }

    JSON::Object jsonObject;

    if (ObjectToJSON(cls, target, jsonObject))
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
