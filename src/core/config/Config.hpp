/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONFIG2_HPP
#define HYPERION_CONFIG2_HPP

#include <core/Defines.hpp>

#include <core/containers/String.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>
#include <core/filesystem/DataStore.hpp>

#include <core/memory/NotNullPtr.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/json/JSON.hpp>

#include <Types.hpp>

#include <HashCode.hpp>

namespace hyperion {

class ByteWriter;
class BufferedReader;

namespace config {

using ConfigurationValue = json::JSONValue;

class ConfigurationTable;

template <class Derived>
class ConfigBase;

class ConfigurationValueKey
{
public:
    ConfigurationValueKey() = default;

    ConfigurationValueKey(const String& path)
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

    HYP_FORCE_INLINE bool operator==(const ConfigurationValueKey& other) const
    {
        return m_path == other.m_path;
    }

    HYP_FORCE_INLINE bool operator!=(const ConfigurationValueKey& other) const
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

class HYP_API ConfigurationTable
{
    template <class Derived>
    friend class ConfigBase;

protected:
    ConfigurationTable();
    ConfigurationTable(const String& config_name, const HypClass* hyp_class);

public:
    ConfigurationTable(const String& config_name);
    ConfigurationTable(const String& config_name, const String& subobject_path);
    ConfigurationTable(const ConfigurationTable& other);
    ConfigurationTable& operator=(const ConfigurationTable& other);
    ConfigurationTable(ConfigurationTable&& other) noexcept;
    ConfigurationTable& operator=(ConfigurationTable&& other) noexcept;
    virtual ~ConfigurationTable() = default;

    bool IsChanged() const;

    ConfigurationTable& Merge(const ConfigurationTable& other);

    HYP_FORCE_INLINE const ConfigurationValue& operator[](UTF8StringView key) const
    {
        return Get(key);
    }

    const ConfigurationValue& Get(UTF8StringView key) const;
    void Set(UTF8StringView key, const ConfigurationValue& value);

    bool Save();

    void AddError(const Error& error);

    HYP_FORCE_INLINE String ToString() const
    {
        return GetSubobject().ToString(true);
    }

protected:
    static const String& GetDefaultConfigName(const HypClass* hyp_class);

    FilePath GetFilePath() const;

    Result Read(json::JSONValue& out_value) const;
    Result Write(const json::JSONValue& value) const;

    void LogErrors() const;
    void LogErrors(UTF8StringView message) const;

    bool SetHypClassFields(const HypClass* hyp_class, const void* ptr);

    bool Validate() const
    {
        return true;
    }

    void PostLoadCallback()
    {
    }

    Optional<String> m_subobject_path;
    json::JSONValue m_root_object;

private:
    json::JSONValue& GetSubobject();
    const json::JSONValue& GetSubobject() const;

    String m_name;
    Array<Error> m_errors;

    mutable HashCode m_cached_hash_code;
};

template <class Derived>
class ConfigBase : public ConfigurationTable
{
    static const ConfigBase<Derived>& GetInstance()
    {
        static const Derived instance {};

        return instance;
    }

protected:
    ConfigBase() = default;

    ConfigBase(const String& config_name)
        : ConfigurationTable(config_name, GetHypClass())
    {
    }

public:
    ConfigBase(const ConfigBase& other) = default;
    ConfigBase& operator=(const ConfigBase& other) = default;
    ConfigBase(ConfigBase&& other) noexcept = default;
    ConfigBase& operator=(ConfigBase&& other) noexcept = default;
    virtual ~ConfigBase() = default;

    static Derived FromConfig()
    {
        if (const String& config_name = GetDefaultConfigName(GetHypClass()); config_name.Any())
        {
            return FromConfig(config_name);
        }

        return FromConfig(TypeNameHelper<Derived, true>::value.Data());
    }

    static Derived FromConfig(const String& config_name)
    {
        if (config_name.Empty())
        {
            // @TODO Log error
            return {};
        }

        const HypClass* hyp_class = GetHypClass();

        Derived result;
        static_cast<ConfigurationTable&>(result) = ConfigurationTable { config_name, hyp_class };

        if (hyp_class)
        {
            static_cast<ConfigurationTable&>(result).SetHypClassFields(hyp_class, &result);
        }

        result.PostLoadCallback();

        if (!result.Validate())
        {
            result.LogErrors("Validation failed");

            return {};
        }

        if (result.IsChanged())
        {
            const bool save_result = result.Save();

            if (!save_result)
            {
                result.LogErrors("Failed to save configuration");
            }
        }

        return result;
    }

private:
    HYP_FORCE_INLINE static const HypClass* GetHypClass()
    {
        return GetClass(TypeId::ForType<Derived>());
    }
};

class GlobalConfig final : public ConfigBase<GlobalConfig>
{
public:
    GlobalConfig()
        : ConfigBase<GlobalConfig>()
    {
    }

    GlobalConfig(const String& config_name)
        : ConfigBase<GlobalConfig>(config_name)
    {
    }

    GlobalConfig(const GlobalConfig& other)
        : ConfigBase<GlobalConfig>(static_cast<const ConfigBase<GlobalConfig>&>(other))
    {
    }

    GlobalConfig& operator=(const GlobalConfig& other) = delete;
    GlobalConfig(GlobalConfig&& other) noexcept = default;
    GlobalConfig& operator=(GlobalConfig&& other) noexcept = delete;
    virtual ~GlobalConfig() override = default;

    HYP_FORCE_INLINE const ConfigurationValue& Get(UTF8StringView key) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return ConfigBase<GlobalConfig>::Get(key);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView key, const ConfigurationValue& value)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        ConfigBase<GlobalConfig>::Set(key, value);
    }

private:
    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace config

using config::ConfigBase;
using config::ConfigurationTable;
using config::ConfigurationValue;
using config::ConfigurationValueKey;
using config::GlobalConfig;

} // namespace hyperion

#endif