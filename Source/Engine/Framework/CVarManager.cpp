/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/Threading/AtomicVar.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Engine);

static AtomicVar<int> s_nextCVarId = 0;
static CVarManager* s_pInstance = nullptr;

extern uint32 GetFrameCounter();

#pragma region CVar

// String specializations

template <>
CVar<const char*>::~CVar()
{
    if (m_value != nullptr)
    {
        Memory::Free(const_cast<char*>(m_value));
    }
}

template <>
void CVar<const char*>::Set(const char* value)
{
    if (m_value != nullptr)
    {
        Memory::Free(const_cast<char*>(m_value));
        m_value = nullptr;
    }

    if (value != nullptr)
    {
        size_t length = Memory::StrLen(value) + 1;
        char* newValue = (char*)Memory::AllocateZeros(length + 1);
        Memory::StrCpy(newValue, value, length);
        m_value = newValue;
    }
}

template <>
bool CVar<const char*>::SetFromBoxed(const BoxedValue& boxed)
{
    if (!boxed.Is<String>())
    {
        return false;
    }

    String& str = boxed.Get<String>();

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

template <>
bool CVar<const char*>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsString())
        return false;

    String str = cfgValue.ToString();

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

// Numeric specializations

template <>
bool CVar<int8>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToInt8();

    return true;
}

template <>
bool CVar<int16>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToInt16();

    return true;
}

template <>
bool CVar<int32>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToInt32();

    return true;
}

template <>
bool CVar<int64>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToInt64();

    return true;
}

template <>
bool CVar<uint8>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToUInt8();

    return true;
}

template <>
bool CVar<uint16>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToUInt16();

    return true;
}

template <>
bool CVar<uint32>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToUInt32();

    return true;
}

template <>
bool CVar<uint64>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToUInt64();

    return true;
}

template <>
bool CVar<float>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToFloat();

    return true;
}

template <>
bool CVar<double>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsNumber())
        return false;

    m_value = cfgValue.ToDouble();

    return true;
}

template <>
bool CVar<bool>::SetFromConfig(const ConfigValue& cfgValue)
{
    if (!cfgValue.IsBool())
        return false;

    m_value = cfgValue.ToBool();

    return true;
}

template <typename T>
inline T ReadCVarValue(const CVar<T>& cvar)
{
    const CVarSnapshot& snapshot = s_pInstance->GetCurrentSnapshot();

    if (HYP_UNLIKELY(cvar.id < 0 || cvar.id >= snapshot.numVars))
    {
        return cvar.m_value;
    }

    return snapshot.values[cvar.id].template GetUnchecked<T>();
}

template <> int8 CVar<int8>::Get() const { return ReadCVarValue(*this); }
template <> int16 CVar<int16>::Get() const { return ReadCVarValue(*this); }
template <> int32 CVar<int32>::Get() const { return ReadCVarValue(*this); }
template <> int64 CVar<int64>::Get() const { return ReadCVarValue(*this); }

template <> uint8 CVar<uint8>::Get() const { return ReadCVarValue(*this); }
template <> uint16 CVar<uint16>::Get() const { return ReadCVarValue(*this); }
template <> uint32 CVar<uint32>::Get() const { return ReadCVarValue(*this); }
template <> uint64 CVar<uint64>::Get() const { return ReadCVarValue(*this); }

template <> float CVar<float>::Get() const { return ReadCVarValue(*this); }
template <> double CVar<double>::Get() const { return ReadCVarValue(*this); }

template <> bool CVar<bool>::Get() const { return ReadCVarValue(*this); }

template <> const char* CVar<const char*>::Get() const { return ReadCVarValue(*this); }

#pragma endregion CVar

#pragma region CVarBase

struct DeferredInitCVar
{
    CVarBase* cvar;
    String path;
    String configPath;
};

static Array<DeferredInitCVar>& GetDeferredInitCVars()
{
    static Array<DeferredInitCVar> s_deferredInitCVars;
    return s_deferredInitCVars;
}

static void InitCVar(CVarManager* manager, CVarBase* cvar, const UTF8StringView& path, const UTF8StringView& configPath = {})
{
    AssertDebug(cvar != nullptr);

    if (!manager)
    {
        DeferredInitCVar& deferredCVar = GetDeferredInitCVars().EmplaceBack();
        deferredCVar.cvar = cvar;
        deferredCVar.path = path;
        deferredCVar.configPath = configPath;

        return;
    }

    cvar->name = CreateNameFromDynamicString(path);
    cvar->id = s_nextCVarId.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

    Assert(cvar->id < MaxCVars);

    if (cvar->id < MaxCVars)
    {
        manager->cvars[cvar->id] = cvar;

        if (configPath)
        {
            const size_t len = Memory::StrLen(configPath.Data()) + 1;
            char* copiedPath = (char*)Memory::AllocateZeros(len);
            Memory::StrCpy(copiedPath, configPath.Data(), len);
            manager->cvarToConfigPath[cvar->id] = copiedPath;
        }
    }
}

CVarBase::CVarBase(const UTF8StringView& path, const UTF8StringView& configPath)
    : id(-1),
      isHeapAllocated(false)
{
    InitCVar(s_pInstance, this, path, configPath);
}

#pragma endregion CVarBase

#pragma region CVarManager

CVarManager& CVarManager::GetInstance()
{
    static CVarManager s_instance;
    return s_instance;
}

CVarManager::CVarManager()
{
    s_pInstance = this;

    cvars.Resize(MaxCVars);
    cvarToConfigPath.Resize(MaxCVars);

    Array<DeferredInitCVar>& deferredInitCVars = GetDeferredInitCVars();

    if (deferredInitCVars.Any())
    {
        for (DeferredInitCVar& deferredCVar : deferredInitCVars)
        {
            InitCVar(this, deferredCVar.cvar, UTF8StringView(deferredCVar.path), UTF8StringView(deferredCVar.configPath));
        }

        deferredInitCVars.Clear();
    }

    m_snapshots[0].version = 1;
}

CVarManager::~CVarManager()
{
    for (CVarBase* var : cvars)
    {
        if (var && var->isHeapAllocated)
        {
            delete var;
        }
    }

    for (const char* path : cvarToConfigPath)
    {
        if (path != nullptr)
        {
            Memory::Free(const_cast<char*>(path));
        }
    }

    s_pInstance = nullptr;
}

void CVarManager::InitFromConfig(const ConfigBase& config)
{
    const int numVars = s_nextCVarId.Get(MemoryOrder::ACQUIRE);

    for (int i = 0; i < numVars; i++)
    {
        CVarBase* cvar = cvars[i];

        if (!cvar)
        {
            continue;
        }

        const char* path = cvarToConfigPath[cvar->id] != nullptr
            ? cvarToConfigPath[cvar->id]
            : cvar->name.LookupString();

        if (!path || path[0] == '\0')
        {
            // invalid name, skip
            HYP_LOG(Engine, Warning, "Invalid cvar name: {}", path);
            continue;
        }

        const ConfigValue& value = config.Get(path);

        if (value.IsNullOrUndefined())
        {
            continue;
        }

        cvar->SetFromConfig(value);
    }
}

HYP_NODISCARD CVarBase* CVarManager::FindVar(const ANSIString& name) const
{
    const int idx = FindVarIndex(name);
    if (idx < 0)
        return nullptr;

    return cvars[idx];
}

template <typename T>
void CVarManager::SetVar(StringHash nameHash, T value)
{
    int idx = FindVarIndex(nameHash);

    if (idx < 0)
    {
        return;
    }

    static_cast<CVar<T>*>(cvars[idx])->Set(value);
}

template <typename T>
T CVarManager::GetVar(StringHash nameHash) const
{
    int idx = FindVarIndex(nameHash);

    if (idx < 0)
    {
        return T {};
    }

    const uint32 snapshotIndex = GetFrameCounter() % RingBufferDepth;
    const CVarSnapshot& snapshot = m_snapshots[snapshotIndex];

    if (idx >= snapshot.numVars)
    {
        return T {};
    }

    return snapshot.values[idx].Get<T>();
}

void CVarManager::Advance()
{
    Mutex::Guard lock(m_mutex);

    const uint32 fc = GetFrameCounter();

    const uint32 currentIdx = fc % RingBufferDepth;
    const uint32 nextIdx = (fc + 1) % RingBufferDepth;

    CVarSnapshot& next = m_snapshots[nextIdx];

    const int numVars = s_nextCVarId.Get(MemoryOrder::ACQUIRE);

    for (int i = 0; i < numVars; i++)
    {
        if (cvars[i])
        {
            cvars[i]->WriteToSnapshot(next.values[i]);
        }
    }

    next.numVars = numVars;
    next.version = m_snapshots[currentIdx].version + 1;
}

const CVarSnapshot& CVarManager::GetCurrentSnapshot() const
{
    return m_snapshots[GetFrameCounter() % RingBufferDepth];
}

HYP_NODISCARD int CVarManager::FindVarIndex(const ANSIString& name) const
{
    ANSIString inNameLower = name.ToLower();

    for (uint32 i = 0; i < MaxCVars; i++)
    {
        if (!cvars[i])
            continue;

        const ANSIString varNameLower = ANSIString(*cvars[i]->name).ToLower();

        if (varNameLower == inNameLower)
            return int(i);

        // 'Foo.Bar.Test' should match with 'Test' as input name.
        const size_t lastSeparatorIndex = varNameLower.FindLastIndex('.');

        if (lastSeparatorIndex != ANSIString::NotFound && varNameLower.Substr(lastSeparatorIndex + 1) == inNameLower)
            return int(i);
    }

    return -1;
}

HYP_NODISCARD int CVarManager::FindVarIndex(StringHash nameHash) const
{
    for (uint32 i = 0; i < MaxCVars; i++)
    {
        if (cvars[i] && cvars[i]->name == nameHash)
        {
            return int(i);
        }
    }

    return -1;
}

#pragma endregion CVarManager

#pragma region Explicit template instantiations

// SetVar

template void CVarManager::SetVar<int8>(StringHash, int8);
template void CVarManager::SetVar<int16>(StringHash, int16);
template void CVarManager::SetVar<int32>(StringHash, int32);
template void CVarManager::SetVar<int64>(StringHash, int64);

template void CVarManager::SetVar<uint8>(StringHash, uint8);
template void CVarManager::SetVar<uint16>(StringHash, uint16);
template void CVarManager::SetVar<uint32>(StringHash, uint32);
template void CVarManager::SetVar<uint64>(StringHash, uint64);

template void CVarManager::SetVar<float>(StringHash, float);
template void CVarManager::SetVar<double>(StringHash, double);

template void CVarManager::SetVar<bool>(StringHash, bool);

template void CVarManager::SetVar<const char*>(StringHash, const char*);

// GetVar

template int8 CVarManager::GetVar<int8>(StringHash) const;
template int16 CVarManager::GetVar<int16>(StringHash) const;
template int32 CVarManager::GetVar<int32>(StringHash) const;
template int64 CVarManager::GetVar<int64>(StringHash) const;

template uint8 CVarManager::GetVar<uint8>(StringHash) const;
template uint16 CVarManager::GetVar<uint16>(StringHash) const;
template uint32 CVarManager::GetVar<uint32>(StringHash) const;
template uint64 CVarManager::GetVar<uint64>(StringHash) const;

template float CVarManager::GetVar<float>(StringHash) const;
template double CVarManager::GetVar<double>(StringHash) const;

template bool CVarManager::GetVar<bool>(StringHash) const;
template String CVarManager::GetVar<String>(StringHash) const;

#pragma endregion Explicit template instantiations

} // namespace Hyperion
