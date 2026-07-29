/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/Util.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <type_traits>

namespace Hyperion {

static constexpr uint32 MaxCVars = 128;

namespace DataProcessing::JSON {
// Fwd declaration for ConfigValue
class Value;
} // namespace DataProcessing::JSON

namespace JSON = DataProcessing::JSON;

namespace config {
class ConfigBase;
using ConfigValue = JSON::Value;
} // namespace config

using config::ConfigBase;
using config::ConfigValue;

struct BoxedValue;

class CVarManager;

using CVarString = const char*;

using CVarSnapshotValue = Variant<
    int8, int16, int32, int64,
    uint8, uint16, uint32, uint64,
    float, double,
    bool,
    CVarString>;

class ENGINE_API CVarBase
{
public:
    friend class CVarManager;

protected:
    explicit CVarBase(const UTF8StringView& path, const UTF8StringView& configPath = {});

public:
    Name name;
    int id;
    bool isHeapAllocated;

    CVarBase(const CVarBase& other) = delete;
    CVarBase& operator=(const CVarBase& other) = delete;

    virtual ~CVarBase() = default;

    virtual bool SetFromConfig(const ConfigValue& cfgValue) = 0;
    virtual bool SetFromBoxed(const BoxedValue& boxed) = 0;
    virtual bool SetFromString(const String& str) = 0;

protected:
    virtual void WriteToSnapshot(CVarSnapshotValue& snapshotValue) const = 0;
};

namespace Detail {

template <typename T>
HYP_FORCE_INLINE T AcquireCVarValue(T value) noexcept
{
    return value;
}

// const char* cvars own a heap copy of their value (the destructor frees it), so even a string
// literal default must be duplicated rather than stored directly.
ENGINE_API CVarString AcquireCVarValue(CVarString value);

} // namespace Detail

template <typename T>
class CVar final : public CVarBase
{
public:
    explicit CVar(const UTF8StringView& path, T defaultValue = T {}, const UTF8StringView& configPath = {})
        : CVarBase(path, configPath),
          m_value(Detail::AcquireCVarValue(defaultValue))
    {
    }

    ~CVar();

    void Set(T value);
    T Get() const;

    explicit HYP_FORCE_INLINE operator T() const
    {
        return Get();
    }

    CVar& operator=(const T& value)
    {
        Set(value);
        return *this;
    }

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

template <>
ENGINE_API CVar<CVarString>::~CVar();

template <>
ENGINE_API void CVar<CVarString>::Set(CVarString value);

#pragma endregion const char* CVar specializations

#pragma region Non- const char* impls

template <typename T>
inline CVar<T>::~CVar() = default;

template <typename T>
inline void CVar<T>::Set(T value)
{
    m_value = value;
}

#pragma endregion Non - const char* impls

#pragma region SetFromConfig specializations

// SetFromConfig

template <>
ENGINE_API bool CVar<int8>::SetFromConfig(const ConfigValue& cfgValue);
template <>
ENGINE_API bool CVar<int16>::SetFromConfig(const ConfigValue& cfgValue);
template <>
ENGINE_API bool CVar<int32>::SetFromConfig(const ConfigValue& cfgValue);
template <>
ENGINE_API bool CVar<int64>::SetFromConfig(const ConfigValue& cfgValue);

template <>
ENGINE_API bool CVar<uint8>::SetFromConfig(const ConfigValue& cfgValue);
template <>
ENGINE_API bool CVar<uint16>::SetFromConfig(const ConfigValue& cfgValue);
template <>
ENGINE_API bool CVar<uint32>::SetFromConfig(const ConfigValue& cfgValue);
template <>
ENGINE_API bool CVar<uint64>::SetFromConfig(const ConfigValue& cfgValue);

template <>
ENGINE_API bool CVar<float>::SetFromConfig(const ConfigValue& cfgValue);
template <>
ENGINE_API bool CVar<double>::SetFromConfig(const ConfigValue& cfgValue);

template <>
ENGINE_API bool CVar<bool>::SetFromConfig(const ConfigValue& cfgValue);

template <>
ENGINE_API bool CVar<CVarString>::SetFromConfig(const ConfigValue& cfgValue);

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

template <>
ENGINE_API bool CVar<CVarString>::SetFromBoxed(const BoxedValue& boxed);

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
inline bool CVar<CVarString>::SetFromString(const String& str)
{
    if (m_value != nullptr)
    {
        Memory::Free(const_cast<char*>(m_value));
        m_value = nullptr;
    }

    char* chars = (char*)Memory::AllocateZeros(str.Size() + 1);
    Memory::CopyString(chars, str.Data(), str.Size());

    if (m_value != nullptr)
    {
        Memory::Free(const_cast<char*>(m_value));
    }

    m_value = chars;

    return true;
}

#pragma endregion SetFromConfig specializations

#pragma region CVarSnapshot

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

#pragma endregion CVarSnapshot

#pragma region CVarManager

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

    /*! \brief Publishes the cvar states so they're visible to other threads */
    void Publish(uint8 ringIndex);
    
    HYP_FORCE_INLINE const CVarSnapshot& GetCurrentSnapshot() const
    {
        if constexpr (UseRingBuffer)
        {
            return GetCurrentSnapshot_Internal();
        }
        else
        {
            return m_snapshots[0];
        }
    }

    Array<CVarBase*> cvars;
    Array<const char*> cvarToConfigPath;

private:
    const CVarSnapshot& GetCurrentSnapshot_Internal() const;

    HYP_NODISCARD int FindVarIndex(const ANSIString& name) const;
    HYP_NODISCARD int FindVarIndex(StringHash nameHash) const;

    CVarSnapshot m_snapshots[RingBufferDepth];
};

#pragma endregion CVarManager

#pragma region CVar (Again)

#pragma region Get specializations

template <typename T>
static inline T ReadCVarValue(const CVar<T>& cvar)
{
    static const CVarManager& s_instance = CVarManager::GetInstance();

    const CVarSnapshot& snapshot = s_instance.GetCurrentSnapshot();

    if (HYP_UNLIKELY(cvar.id < 0 || cvar.id >= snapshot.numVars))
    {
        return cvar.m_value;
    }

    return snapshot.values[cvar.id].template GetUnchecked<T>();
}

#define DEF_CVAR_GET(T) \
    template <> \
    inline T CVar<T>::Get() const { return ReadCVarValue(*this); }

DEF_CVAR_GET(int8);
DEF_CVAR_GET(int16);
DEF_CVAR_GET(int32);
DEF_CVAR_GET(int64);
DEF_CVAR_GET(uint8);
DEF_CVAR_GET(uint16);
DEF_CVAR_GET(uint32);
DEF_CVAR_GET(uint64);
DEF_CVAR_GET(float);
DEF_CVAR_GET(double);
DEF_CVAR_GET(bool);
DEF_CVAR_GET(CVarString);

#undef DEF_CVAR_GET

#pragma endregion Get specializations

#pragma endregion CVar (Again)

} // namespace Hyperion
