/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Name.hpp>

#include <core/utilities/Uuid.hpp>
#include <core/utilities/Format.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

// false while in static initialization to disable mutex locking; set to true on engine startup
static bool g_isNameRegistryInitialized = false;

#pragma region NameRegistry

class NameRegistry
{
public:
    using Bucket = typename HashMap<NameID, Pair<ANSIString, uint32>>::Bucket;
    using Node = typename HashMap<NameID, Pair<ANSIString, uint32>>::Node;

    static Bucket* g_nullBucket;
    static Node* g_nullNode;

    NameRegistry() = default;
    NameRegistry(const NameRegistry& other) = delete;
    NameRegistry& operator=(const NameRegistry& other) = delete;
    NameRegistry(NameRegistry&& other) noexcept = delete;
    NameRegistry& operator=(NameRegistry&& other) noexcept = delete;
    ~NameRegistry() = default;

    Name RegisterName(NameID id, const ANSIString& str, bool lock);
    Name RegisterUniqueName(const ANSIString& str, bool lock);
    const ANSIString& LookupStringForName(Name name) const;

private:
    HashMap<NameID, Pair<ANSIString, uint32>> m_nameMap;
    mutable Mutex m_mutex;
};

NameRegistry::Bucket* NameRegistry::g_nullBucket = nullptr;
NameRegistry::Node* NameRegistry::g_nullNode = nullptr;

Name NameRegistry::RegisterName(NameID id, const ANSIString& str, bool lock)
{
    Name name(id);

    if (!g_isNameRegistryInitialized)
    {
        lock = false;
    }

    if (lock)
    {
        m_mutex.Lock();
    }

    auto it = m_nameMap.Find(id);

    if (it != m_nameMap.End())
    {
        Pair<ANSIString, uint32>& stringCountPair = it->second;

        ++stringCountPair.second;
    }
    else
    {
        m_nameMap.Insert({ id, Pair<ANSIString, uint32> { str, 1 } });
    }

    if (lock)
    {
        m_mutex.Unlock();
    }

    return name;
}

Name NameRegistry::RegisterUniqueName(const ANSIString& str, bool lock)
{
    Name name;
    bool inserted = false;
    int suffix = 0;

    if (!g_isNameRegistryInitialized)
    {
        lock = false;
    }

    if (lock)
    {
        m_mutex.Lock();
    }

    while (!inserted)
    {
        ANSIString strWithSuffix = str;

        if (suffix > 0)
        {
            strWithSuffix = ANSIString(HYP_FORMAT("{}_{}", str, suffix));
        }

        const NameID nameIdWithSuffix = NameRegistration::GenerateID(strWithSuffix);

        auto it = m_nameMap.Find(nameIdWithSuffix);

        if (it == m_nameMap.End())
        {
            m_nameMap.Insert({ nameIdWithSuffix,
                Pair<ANSIString, uint32> { strWithSuffix, 1 } });

            name = Name(nameIdWithSuffix);

            break;
        }

        ++suffix;
    }

    if (lock)
    {
        m_mutex.Unlock();
    }

    return name;
}

const ANSIString& NameRegistry::LookupStringForName(Name name) const
{
    if (!name.IsValid())
    {
        return ANSIString::empty;
    }

    Mutex::Guard guard(m_mutex);

    const auto it = m_nameMap.Find(name.hashCode);

    if (it == m_nameMap.End())
    {
        return ANSIString::empty;
    }

    return it->second.first;
}

HYP_API Name RegisterName(NameRegistry* nameRegistry, NameID id, const ANSIString& str, bool lock)
{
    return nameRegistry->RegisterName(id, str, lock);
}

HYP_API const ANSIString& LookupStringForName(const NameRegistry* nameRegistry, Name name)
{
    return nameRegistry->LookupStringForName(name);
}

HYP_API bool ShouldLockNameRegistry()
{
    return g_isNameRegistryInitialized;
}

HYP_API void InitializeNameRegistry()
{
    g_isNameRegistryInitialized = true;
}

#pragma endregion NameRegistry

#pragma region Name

NameRegistry* Name::s_registry = nullptr;

NameRegistry* Name::GetRegistry()
{
    static NameRegistry nameRegistry;

    return (s_registry = &nameRegistry);
}

Name Name::Unique(ANSIStringView prefix)
{
    return GetRegistry()->RegisterUniqueName(prefix, /* lock */ true);
}

const char* Name::LookupString() const
{
    return GetRegistry()->LookupStringForName(*this).Data();
}

#pragma endregion Name

Name CreateNameFromDynamicString(const ANSIString& str)
{
    const NameRegistration nameRegistration = NameRegistration::FromDynamicString(str);

    return Name(nameRegistration.id);
}

WeakName CreateWeakNameFromDynamicString(const ANSIStringView& str)
{
    return WeakName(NameRegistration::GenerateID(str));
}

#pragma region NameRegistration

NameID NameRegistration::GenerateID(const ANSIStringView& str)
{
    const HashCode hashCode = HashCode::GetHashCode(str.Data());
    const NameID id = hashCode.Value();

    return id;
}

NameRegistration NameRegistration::FromDynamicString(const ANSIString& str)
{
    const NameID id = GenerateID(str);

    Name::GetRegistry()->RegisterName(id, str, ShouldLockNameRegistry());

    return NameRegistration { id };
}

#pragma endregion NameRegistration

} // namespace hyperion
