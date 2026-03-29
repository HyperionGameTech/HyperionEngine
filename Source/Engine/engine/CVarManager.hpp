/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/Util.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/name/Name.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <type_traits>

namespace Hyperion {

static constexpr uint32 MaxCVars = 128;

namespace JSON {
// Fwd declaration for ConfigValue
class Value;
} // namespace JSON

namespace config {
class ConfigBase;
using ConfigValue = JSON::Value;
} // namespace config

using config::ConfigValue;
using config::ConfigBase;

struct BoxedValue;

class CVarManager;

using CVarSnapshotValue = Variant<
    int8, int16, int32, int64,
    uint8, uint16, uint32, uint64,
    float, double,
    bool,
    const char*>;

class CVarBase
{
public:
    friend class CVarManager;

protected:
    explicit CVarBase(const UTF8StringView& path, const UTF8StringView& configPath = {});
    ~CVarBase() = default;

public:
    Name name;
    int id;
    bool isHeapAllocated;
    UTF8StringView configPath;

    CVarBase(const CVarBase& other) = delete;
    CVarBase& operator=(const CVarBase& other) = delete;

    virtual bool SetFromConfig(const ConfigValue& cfgValue) = 0;
    virtual bool SetFromBoxed(const BoxedValue& boxed) = 0;
    virtual bool SetFromString(const String& str) = 0;
    
protected:
    virtual void WriteToSnapshot(CVarSnapshotValue& snapshotValue) const = 0;
};

template <typename T>
class CVar final : public CVarBase
{
public:
    explicit CVar(const UTF8StringView& path, T defaultValue = T {}, const UTF8StringView& configPath = {})
        : CVarBase(path, configPath),
          m_value(defaultValue)
    {
    }

    ~CVar();

    void Set(T value);
    T Get() const;

    bool SetFromConfig(const ConfigValue& cfgValue) override;
    bool SetFromBoxed(const BoxedValue& boxed) override;
    bool SetFromString(const String& str) override;

protected:
    void WriteToSnapshot(CVarSnapshotValue& snapshotValue) const override
    {
        snapshotValue.Set(m_value);
    }

private:
    template <typename U>
    friend U ReadCVarValue(const CVar<U>& cvar);

    T m_value;
};

#pragma region const char* CVar specializations

// We need specific impls for dtor and Set() for const char* to handle the dynamic memory allocation of the string.

template <> HYP_API CVar<const char*>::~CVar();
template <> HYP_API void CVar<const char*>::Set(const char* value);

#pragma endregion const char* CVar specializations

#pragma region Non- const char* impls

template <typename T>
inline CVar<T>::~CVar() = default;

template <typename T>
inline void CVar<T>::Set(T value)
{
    m_value = value;
}

#pragma endregion Non- const char* impls

#pragma region SetFromConfig specializations

// SetFromConfig

template <> HYP_API bool CVar<int8>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API bool CVar<int16>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API bool CVar<int32>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API bool CVar<int64>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API bool CVar<uint8>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API bool CVar<uint16>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API bool CVar<uint32>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API bool CVar<uint64>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API bool CVar<float>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API bool CVar<double>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API bool CVar<bool>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API bool CVar<const char*>::SetFromConfig(const ConfigValue& cfgValue);

// SetFromBoxed

template <typename T>
inline bool CVar<T>::SetFromBoxed(const BoxedValue& boxed)
{
    if (!boxed.Is<T>())
    {
        return false;
    }
    
    m_value = boxed.Get<T>();

    return true;
}

template <> HYP_API bool CVar<const char*>::SetFromBoxed(const BoxedValue& boxed);

// SetFromString

// default impl
template <typename T>
inline bool CVar<T>::SetFromString(const String& str)
{
    return false;
}

template <>
inline bool CVar<int8>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<int16>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<int32>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<int64>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<uint8>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<uint16>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<uint32>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<uint64>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<float>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<double>::SetFromString(const String& str)
{
    return StringUtil::Parse(str, &m_value);
}

template <>
inline bool CVar<bool>::SetFromString(const String& str)
{
    if (str == "true" || str == "1")
    {
        m_value = true;
        return true;
    }
    else if (str == "false" || str == "0")
    {
        m_value = false;
        return true;
    }

    return false;
}

template <>
inline bool CVar<const char*>::SetFromString(const String& str)
{
    if (m_value != nullptr)
    {
        Memory::Free(const_cast<char*>(m_value));
        m_value = nullptr;
    }

    char* chars = (char*)Memory::AllocateZeros(str.Size() + 1);
    Memory::StrCpy(chars, str.Data(), str.Size());

    if (m_value != nullptr)
    {
        Memory::Free(const_cast<char*>(m_value));
    }

    m_value = chars;

    return true;
}

#pragma endregion SetFromConfig specializations

#pragma region Get specializations

template <> int8 CVar<int8>::Get() const;
template <> int16 CVar<int16>::Get() const;
template <> int32 CVar<int32>::Get() const;
template <> int64 CVar<int64>::Get() const;

template <> uint8 CVar<uint8>::Get() const;
template <> uint16 CVar<uint16>::Get() const;
template <> uint32 CVar<uint32>::Get() const;
template <> uint64 CVar<uint64>::Get() const;

template <> float CVar<float>::Get() const;
template <> double CVar<double>::Get() const;

template <> bool CVar<bool>::Get() const;

template <> const char* CVar<const char*>::Get() const;

#pragma endregion Get specializations

struct CVarSnapshot
{
    CVarSnapshotValue values[MaxCVars];
    int numVars;
    uint32 version;

    CVarSnapshot()
        : values {},
          numVars(0),
          version(0)
    {
    }
};

class CVarManager
{
public:
    static CVarManager& GetInstance();

    CVarManager();
    ~CVarManager();

    void InitFromConfig(const ConfigBase& config);

    HYP_NODISCARD CVarBase* FindVar(const ANSIString& name) const;

    template <typename T>
    void SetVar(StringHash nameHash, T value);

    template <typename T>
    T GetVar(StringHash nameHash) const;

    /*! \brief Publishes the cvar states so they're visible to other threads.
     *  Call once per frame at end of frame. */
    void Advance();

    const CVarSnapshot& GetCurrentSnapshot() const;

    FixedArray<CVarBase*, MaxCVars> vars;

private:
    HYP_NODISCARD int FindVarIndex(const ANSIString& name) const;
    HYP_NODISCARD int FindVarIndex(StringHash nameHash) const;

    CVarSnapshot m_snapshots[RingBufferDepth];
    AtomicVar<uint32> m_snapshotIndex;
    Mutex m_mutex;
};

} // namespace Hyperion
