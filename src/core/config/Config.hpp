/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONFIG2_HPP
#define HYPERION_CONFIG2_HPP

#include <core/Defines.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/StringView.hpp>
#include <core/filesystem/FilePath.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <util/json/JSON.hpp>

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

    HYP_FORCE_INLINE constexpr bool IsValid() const
        { return m_path.Any(); }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE constexpr bool operator==(const ConfigurationValueKey &other) const
    {
        return m_path == other.m_path;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const ConfigurationValueKey &other) const
    {
        return m_path != other.m_path;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_path);
    }

private:
    String  m_path;
};

class HYP_API ConfigurationDataStore
{
public:
    ConfigurationDataStore(UTF8StringView config_name);
    ConfigurationDataStore(const ConfigurationDataStore &other)                 = delete;
    ConfigurationDataStore &operator=(const ConfigurationDataStore &other)      = delete;
    ConfigurationDataStore(ConfigurationDataStore &&other) noexcept             = delete;
    ConfigurationDataStore &operator=(ConfigurationDataStore &&other) noexcept  = delete;
    ~ConfigurationDataStore();

    FilePath GetFilePath() const;

    bool Read(json::JSONValue &out_value) const;
    bool Write(const json::JSONValue &value) const;

private:
    const String    m_config_name;
};


/*! \brief A table of configuration values, stored as JSON objects.
 *  \details This class is thread-safe, and can be used to store configuration values that can be
 *           read and written from multiple threads. Use Get() and Set() to read and write values using
 *           an (optionally nested) key, in the format "group.subgroup.key", or simply "key" if the
 *           value is not nested. */
class HYP_API ConfigurationTable
{
public:
    ConfigurationTable(const String &config_name);
    ConfigurationTable(const ConfigurationTable &other)                 = delete;
    ConfigurationTable &operator=(const ConfigurationTable &other)      = delete;
    ConfigurationTable(ConfigurationTable &&other) noexcept             = delete;
    ConfigurationTable &operator=(ConfigurationTable &&other) noexcept  = delete;
    ~ConfigurationTable()                                               = default;

    HYP_FORCE_INLINE const ConfigurationDataStore &GetDataStore() const
        { return m_data_store; }

    const ConfigurationValue &Get(const String &key) const;
    void Set(const String &key, const ConfigurationValue &value);

    bool Save() const;

private:
    ConfigurationDataStore                          m_data_store;

    // static version is a copy of the dynamic version that is updated on calls to Get() when
    // the dynamic version has been changed. Used to avoid locking when reading the configuration.
    mutable json::JSONValue                         m_root_static;

    // dynamic version is the actual configuration that is updated when Set() is called.
    // changing it requires locking the mutex.
    json::JSONValue                                 m_root_dynamic;
    mutable AtomicVar<bool>                         m_is_dirty; // Used to mark the dynamic root as dirty
    mutable Mutex                                   m_mutex;
};

} // namespace config

using config::ConfigurationValue;
using config::ConfigurationValueKey;
using config::ConfigurationTable;

} // namespace hyperion

#endif