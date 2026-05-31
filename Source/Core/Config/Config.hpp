/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Utilities/StringView.hpp>
#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Memory/NotNullPtr.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/JSON/JSON.hpp>

#include <Core/Types.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class ByteWriter;
class BufferedReader;

struct ToJSONOptions;

namespace config {

class ConfigBase;

template <class Derived>
class Config;

class ConfigKey
{
public:
    ConfigKey() = default;

    ConfigKey(const String& path)
        : m_path(path)
    {
    }

    HYP_FORCE_INLINE const String& GetPath() const
    {
        return m_path;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_path.Any();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const ConfigKey& other) const
    {
        return m_path == other.m_path;
    }

    HYP_FORCE_INLINE bool operator!=(const ConfigKey& other) const
    {
        return m_path != other.m_path;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_path);
    }

private:
    String m_path;
};

using ConfigValue = JSON::Value;

class CORE_API ConfigBase
{
    template <class Derived>
    friend class Config;

protected:
    ConfigBase(const String& configName, const Class* cls);

public:
    static Result(*s_ObjectFromJSON)(const JSON::Object& jsonObject, const Class* targetClass, BoxedValue& target);
    static Result(*s_ObjectToJSON)(const Class* cls, const BoxedValue& target, JSON::Object& outJson, ToJSONOptions* pOptions);

    ConfigBase();

    explicit ConfigBase(const String& configName);
    ConfigBase(const String& configName, const String& subobjectPath);

    ConfigBase(const ConfigBase& other);
    ConfigBase& operator=(const ConfigBase& other);

    ConfigBase(ConfigBase&& other) noexcept;
    ConfigBase& operator=(ConfigBase&& other) noexcept;

    virtual ~ConfigBase() = default;

    bool IsChanged() const;

    ConfigBase& Merge(const ConfigBase& other);

    HYP_FORCE_INLINE const ConfigValue& operator[](UTF8StringView key) const
    {
        return Get(key);
    }

    const ConfigValue& Get(UTF8StringView key) const;
    void Set(UTF8StringView key, const ConfigValue& value);

    bool Save();
    bool Load();

    void AddError(const Error& error);

    void LogErrors(FILE* outFile) const;
    void LogErrors(FILE* outFile, UTF8StringView message) const;

    HYP_FORCE_INLINE String ToString() const
    {
        return GetSubobject().ToString(true);
    }

protected:
    static UTF8StringView GetDefaultConfigName(const Class* cls);

    FilePath GetFilePath() const;

    Result Read(JSON::Value& outValue) const;
    Result Write(const JSON::Value& value) const;

    bool SetClassFields(const Class* cls, const void* ptr);

    bool Validate() const
    {
        return true;
    }

    void PostLoadCallback()
    {
    }

    Optional<String> m_subobjectPath;
    JSON::Value m_rootObject;

private:
    JSON::Value& GetSubobject();
    const JSON::Value& GetSubobject() const;

    String m_name;
    Array<Error> m_errors;

    mutable HashCode m_cachedHashCode;
};

template <class Derived>
class Config : public ConfigBase
{
    static const Config<Derived>& GetInstance()
    {
        static const Derived s_instance {};

        return s_instance;
    }

protected:
    Config() = default;

    Config(const String& configName)
        : ConfigBase(configName, GetDerivedClass())
    {
    }

public:
    Config(const Config& other) = default;
    Config& operator=(const Config& other) = default;

    Config(Config&& other) noexcept = default;
    Config& operator=(Config&& other) noexcept = default;

    virtual ~Config() = default;

    static Derived FromConfig()
    {
        if (UTF8StringView configName = GetDefaultConfigName(GetDerivedClass()); configName.Length() > 0)
        {
            return FromConfig(configName);
        }

        return FromConfig(TypeName<Derived>().Data());
    }

    static Derived FromConfig(const String& configName)
    {
        if (configName.Empty())
        {
            return {};
        }

        const Class* cls = GetDerivedClass();

        Derived result {};
        static_cast<ConfigBase&>(result) = ConfigBase { configName, cls };

        if (!result.Load())
        {
            result.LogErrors(stderr, "Failed to load config");
            return {};
        }

        if (cls)
        {
            static_cast<ConfigBase&>(result).SetClassFields(cls, &result);
        }

        result.PostLoadCallback();

        if (!result.Validate())
        {
            result.LogErrors(stderr, "Validation failed");

            return {};
        }

        if (result.IsChanged())
        {
            const bool saveResult = result.Save();

            if (!saveResult)
            {
                result.LogErrors(stderr, "Failed to save configuration");
            }
        }

        return result;
    }

private:
    HYP_FORCE_INLINE static const Class* GetDerivedClass()
    {
        static const TypeId s_derivedTypeId = TypeId::ForType<Derived>();
        return GetClass(s_derivedTypeId);
    }
};

class GlobalConfig final : public Config<GlobalConfig>
{
public:
    GlobalConfig()
        : Config<GlobalConfig>(),
          m_next(nullptr)
    {
    }

    explicit GlobalConfig(const String& configName)
        : Config<GlobalConfig>(configName),
          m_next(nullptr)
    {
    }

    GlobalConfig(const GlobalConfig& other)
        : Config<GlobalConfig>(static_cast<const Config<GlobalConfig>&>(other)),
          m_next(nullptr)
    {
    }

    GlobalConfig& operator=(const GlobalConfig& other) = delete;

    GlobalConfig(GlobalConfig&& other) noexcept
        : Config<GlobalConfig>(static_cast<Config<GlobalConfig>&&>(other)),
          m_next(nullptr)
    {
    }

    GlobalConfig& operator=(GlobalConfig&& other) noexcept = delete;

    ~GlobalConfig() override = default;

    HYP_FORCE_INLINE const ConfigValue& Get(UTF8StringView key) const
    {
        return Config<GlobalConfig>::Get(key);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView key, const ConfigValue& value)
    {
        Config<GlobalConfig>::Set(key, value);
    }

    GlobalConfig* GetNewRevision() const
    {
        TSharedLock lock(m_mutex);
        return m_next;
    }

    void SetNewRevision(GlobalConfig* newRevision)
    {
        TUniqueLock lock(m_mutex);
        m_next = newRevision;
    }

private:
    // Pointer to the latest revision/snapshot.
    GlobalConfig* m_next;
    SharedMutex m_mutex;
};

} // namespace config

using config::Config;
using config::ConfigBase;
using config::ConfigValue;
using config::ConfigKey;
using config::GlobalConfig;

} // namespace Hyperion
