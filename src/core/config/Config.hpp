/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONFIG2_HPP
#define HYPERION_CONFIG2_HPP

#include <core/Defines.hpp>

#include <core/containers/String.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/StringView.hpp>

#include <core/filesystem/FilePath.hpp>
#include <core/filesystem/DataStore.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/json/JSON.hpp>

#include <Types.hpp>

#include <HashCode.hpp>

namespace hyperion {

class ByteWriter;
class BufferedReader;

namespace config {

using ConfigurationValue = json::JSONValue;

class ConfigurationTable;

// struct ConfigurationValueKey
// {
//     String  group;
//     String  key;

//     ConfigurationValueKey()                                                     = default;

//     ConfigurationValueKey(UTF8StringView value)
//     {
//         Array<String> parts = String(value).Split('/');

//         if (parts.Empty()) {
//             return;
//         }

//         group = std::move(parts[0]);

//         if (parts.Size() > 1) {
//             key = std::move(parts[1]);
//         }
//     }

//     ConfigurationValueKey(UTF8StringView group, UTF8StringView key)
//         : group(group),
//           key(key)
//     {
//     }

//     ConfigurationValueKey(const ConfigurationValueKey &other)                   = default;
//     ConfigurationValueKey &operator=(const ConfigurationValueKey &other)        = default;
//     ConfigurationValueKey(ConfigurationValueKey &&other) noexcept               = default;
//     ConfigurationValueKey &operator=(ConfigurationValueKey &&other) noexcept    = default;
//     ~ConfigurationValueKey()                                                    = default;

//     HYP_FORCE_INLINE bool IsValid() const
//     {
//         return group.Any()
//             && key.Any();
//     }

//     HYP_FORCE_INLINE explicit operator bool() const
//         { return IsValid(); }

//     HYP_FORCE_INLINE bool operator==(const ConfigurationValueKey &other) const
//     {
//         return group == other.group
//             && key == other.key;
//     }

//     HYP_FORCE_INLINE bool operator!=(const ConfigurationValueKey &other) const
//     {
//         return group != other.group
//             || key != other.key;
//     }

//     HYP_FORCE_INLINE HashCode GetHashCode() const
//     {
//         return HashCode::GetHashCode(group)
//             .Add(key);
//     }
// };

class ConfigurationValueKey
{
public:
    ConfigurationValueKey() = default;
    ConfigurationValueKey(const String &path)
        : m_path(path)
    {
    }

    HYP_FORCE_INLINE const String &GetPath() const
        { return m_path; }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_path.Any(); }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool operator==(const ConfigurationValueKey &other) const
    {
        return m_path == other.m_path;
    }

    HYP_FORCE_INLINE bool operator!=(const ConfigurationValueKey &other) const
    {
        return m_path != other.m_path;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_path);
    }

private:
    String  m_path;
};

class HYP_API ConfigurationDataStore : public DataStoreBase
{
public:
    ConfigurationDataStore(UTF8StringView config_name);
    ConfigurationDataStore(const ConfigurationDataStore &other)                 = delete;
    ConfigurationDataStore &operator=(const ConfigurationDataStore &other)      = delete;
    ConfigurationDataStore(ConfigurationDataStore &&other) noexcept;
    ConfigurationDataStore &operator=(ConfigurationDataStore &&other) noexcept  = delete;
    virtual ~ConfigurationDataStore() override;

    HYP_FORCE_INLINE const String &GetConfigName() const
        { return m_config_name; }

    FilePath GetFilePath() const;

    bool Read(json::JSONValue &out_value) const;
    bool Write(const json::JSONValue &value) const;

protected:
    virtual Name GetTypeName() const override
        { return NAME("ConfigurationDataStore"); }

private:
    String  m_config_name;
};

class HYP_API ConfigurationTable
{
public:
    ConfigurationTable(const String &config_name);
    ConfigurationTable(const ConfigurationTable &other);
    ConfigurationTable &operator=(const ConfigurationTable &other);
    ConfigurationTable(ConfigurationTable &&other) noexcept;
    ConfigurationTable &operator=(ConfigurationTable &&other) noexcept;
    ~ConfigurationTable()   = default;

    HYP_FORCE_INLINE ConfigurationDataStore *GetDataStore() const
        { return m_data_store; }

    bool IsChanged() const;

    ConfigurationTable &Merge(const ConfigurationTable &other);

    HYP_FORCE_INLINE const ConfigurationValue &operator[](UTF8StringView key) const
        { return Get(key); }

    const ConfigurationValue &Get(UTF8StringView key) const;
    void Set(UTF8StringView key, const ConfigurationValue &value);

    bool Save() const;

    HYP_FORCE_INLINE String ToString() const
        { return m_root_object.ToString(); }

private:
    ConfigurationDataStore                  *m_data_store;
    TResourceHandle<ConfigurationDataStore> m_data_store_resource_handle;

    json::JSONValue                         m_root_object;
    mutable HashCode                        m_cached_hash_code;
};

HYP_API const ConfigurationTable &GetGlobalConfigurationTable();

template <class Derived>
class ConfigBase : public ConfigurationTable
{
    static const ConfigBase<Derived> &GetInstance()
    {
        static const Derived instance { };

        return instance;
    }

protected:
    ConfigBase()
        : ConfigurationTable("<default>")
    {
    }

    ConfigBase(const String &config_name)
        : ConfigurationTable(config_name)
    {
    }

    ConfigBase(const ConfigurationTable &configuration_table)
        : ConfigurationTable(configuration_table)
    {
    }

public:
    ConfigBase(const ConfigBase &other)                 = default;
    ConfigBase &operator=(const ConfigBase &other)      = default;
    ConfigBase(ConfigBase &&other) noexcept             = default;
    ConfigBase &operator=(ConfigBase &&other) noexcept  = default;

    virtual ~ConfigBase()                               = default;

    static Derived FromTemplate(const ConfigurationTable &table)
    {
        Derived result = Default();

        ConfigurationTable merged_table = table;
        merged_table.Merge(result);

        static_cast<ConfigurationTable &>(result) = std::move(merged_table);

        return result;
    }

    static Derived Default()
    {
        static const Derived default_value = GetInstance().Default_Internal();

        return default_value;
    }

    static Derived FromConfig()
    {
        Derived result = GetInstance().FromConfig_Internal();

        ConfigurationTable merged_table = GetGlobalConfigurationTable();
        merged_table.Merge(result);

        static_cast<ConfigurationTable &>(result) = std::move(merged_table);

        return result;
    }

protected:
    virtual Derived Default_Internal() const
        { return FromTemplate(GetGlobalConfigurationTable()); }

    virtual Derived FromConfig_Internal() const = 0;

    virtual bool Validate() const
        { return true; }
};

class GlobalConfig final : public ConfigBase<GlobalConfig>
{
public:
    GlobalConfig()
        : ConfigBase<GlobalConfig>()
    {
    }

    GlobalConfig(const String &config_name)
        : ConfigBase<GlobalConfig>(config_name)
    {
    }

    GlobalConfig(const GlobalConfig &other)
        : ConfigBase<GlobalConfig>(static_cast<const ConfigBase<GlobalConfig> &>(other))
    {
    }
    
    GlobalConfig &operator=(const GlobalConfig &other)      = delete;
    GlobalConfig(GlobalConfig &&other) noexcept             = default;
    GlobalConfig &operator=(GlobalConfig &&other) noexcept  = delete;
    virtual ~GlobalConfig() override                        = default;

    HYP_FORCE_INLINE const ConfigurationValue &Get(UTF8StringView key) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return ConfigBase<GlobalConfig>::Get(key);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView key, const ConfigurationValue &value)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        ConfigBase<GlobalConfig>::Set(key, value);
    }

protected:
    virtual GlobalConfig FromConfig_Internal() const override
    {
        return GlobalConfig(GetDataStore()->GetConfigName());
    }

private:
    DataRaceDetector    m_data_race_detector;
};

} // namespace config

using config::ConfigurationValue;
using config::ConfigurationValueKey;
using config::ConfigurationTable;
using config::ConfigBase;
using config::GlobalConfig;

} // namespace hyperion

#endif