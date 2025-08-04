/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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
    ConfigurationTable(const String& configName, const HypClass* hypClass);

public:
    ConfigurationTable();
    ConfigurationTable(const String& configName);
    ConfigurationTable(const String& configName, const String& subobjectPath);
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
    static const String& GetDefaultConfigName(const HypClass* hypClass);

    FilePath GetFilePath() const;

    Result Read(json::JSONValue& outValue) const;
    Result Write(const json::JSONValue& value) const;

    void LogErrors() const;
    void LogErrors(UTF8StringView message) const;

    bool SetHypClassFields(const HypClass* hypClass, const void* ptr);

    bool Validate() const
    {
        return true;
    }

    void PostLoadCallback()
    {
    }

    Optional<String> m_subobjectPath;
    json::JSONValue m_rootObject;

private:
    json::JSONValue& GetSubobject();
    const json::JSONValue& GetSubobject() const;

    String m_name;
    Array<Error> m_errors;

    mutable HashCode m_cachedHashCode;
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

    ConfigBase(const String& configName)
        : ConfigurationTable(configName, GetHypClass())
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
        if (const String& configName = GetDefaultConfigName(GetHypClass()); configName.Any())
        {
            return FromConfig(configName);
        }

        return FromConfig(TypeNameHelper<Derived, true>::value.Data());
    }

    static Derived FromConfig(const String& configName)
    {
        if (configName.Empty())
        {
            // @TODO Log error
            return {};
        }

        const HypClass* hypClass = GetHypClass();

        Derived result;
        static_cast<ConfigurationTable&>(result) = ConfigurationTable { configName, hypClass };

        if (hypClass)
        {
            static_cast<ConfigurationTable&>(result).SetHypClassFields(hypClass, &result);
        }

        result.PostLoadCallback();

        if (!result.Validate())
        {
            result.LogErrors("Validation failed");

            return {};
        }

        if (result.IsChanged())
        {
            const bool saveResult = result.Save();

            if (!saveResult)
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

    GlobalConfig(const String& configName)
        : ConfigBase<GlobalConfig>(configName)
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
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return ConfigBase<GlobalConfig>::Get(key);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView key, const ConfigurationValue& value)
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        ConfigBase<GlobalConfig>::Set(key, value);
    }

private:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace config

using config::ConfigBase;
using config::ConfigurationTable;
using config::ConfigurationValue;
using config::ConfigurationValueKey;
using config::GlobalConfig;

} // namespace hyperion
