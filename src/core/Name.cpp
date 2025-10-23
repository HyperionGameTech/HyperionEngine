/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Name.hpp>

#include <core/utilities/Uuid.hpp>
#include <core/utilities/Format.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

// false while in static initialization to disable mutex locking; set to true on engine startup
static bool s_isNameRegistryInitialized = false;

static constexpr SizeType NamePoolBlockSize = 1024 * 1024; // 1 MB

static Pool* s_namePool;

static Pool& GetNamePool()
{
    static struct Initializer
    {
        Initializer()
        {
            if (s_namePool != nullptr)
            {
                return;
            }

            // Not PF_THREAD_SAFE, since we use mutexes in NameRegistry for all places that need it anyway
            s_namePool = new Pool(NamePoolBlockSize, PF_NONE, ThreadId::Invalid());
        }
    } s_initializer;

    return *s_namePool;
}

using NameAllocator = AllocatorInstance<Pool, &s_namePool>;

#pragma region NameRegistry

class NameRegistry
{
public:
    using CharBuffer = Array<char, NameAllocator>;
    using MapType = HashMap<NameID, Pair<CharBuffer, uint32>, NodeAllocator<NameAllocator>>;

    using Bucket = typename MapType::Bucket;
    using Node = typename MapType::Node;

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
    const CharBuffer& LookupStringForName(Name name) const;

private:
    MapType m_nameMap;
    mutable Mutex m_mutex;
};

NameRegistry::Bucket* NameRegistry::g_nullBucket = nullptr;
NameRegistry::Node* NameRegistry::g_nullNode = nullptr;

Name NameRegistry::RegisterName(NameID id, const ANSIString& str, bool lock)
{
    Name name(id);

    if (HYP_UNLIKELY(!s_isNameRegistryInitialized))
    {
        (void)GetNamePool(); // ensure name pool is initialized
        lock = false;
    }

    if (lock)
    {
        m_mutex.Lock();
    }

    auto it = m_nameMap.Find(id);

    if (it != m_nameMap.End())
    {
        Pair<CharBuffer, uint32>& p = it->second;

        ++p.second;
    }
    else
    {
        m_nameMap.Insert({ id, Pair<CharBuffer, uint32> { CharBuffer(str.Data(), str.Data() + str.Size() + 1), 1 } });
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

    if (HYP_UNLIKELY(!s_isNameRegistryInitialized))
    {
        (void)GetNamePool(); // ensure name pool is initialized
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
            m_nameMap.Insert({ nameIdWithSuffix, Pair<CharBuffer, uint32> { CharBuffer(strWithSuffix.Data(), strWithSuffix.Data() + strWithSuffix.Size() + 1), 1 } });

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

const NameRegistry::CharBuffer& NameRegistry::LookupStringForName(Name name) const
{
    static const CharBuffer s_emptyString { '\0' };

    if (!name.IsValid())
    {
        return s_emptyString;
    }

    bool locked = false;

    if (ShouldLockNameRegistry())
    {
        m_mutex.Lock();
        locked = true;
    }

    HYP_DEFER({
        if (locked)
        {
            m_mutex.Unlock();
        }
    });

    const auto it = m_nameMap.Find(name.hashCode);

    if (it == m_nameMap.End())
    {
        return s_emptyString;
    }

    return it->second.first;
}

Name RegisterName(NameRegistry* nameRegistry, NameID id, const ANSIString& str, bool lock)
{
    return nameRegistry->RegisterName(id, str, lock);
}

const NameRegistry::CharBuffer& LookupStringForName(const NameRegistry* nameRegistry, Name name)
{
    return nameRegistry->LookupStringForName(name);
}

bool ShouldLockNameRegistry()
{
    return s_isNameRegistryInitialized;
}

void NameRegistry_Initialize()
{
    HYP_CORE_ASSERT(!s_isNameRegistryInitialized, "NameRegistry is already initialized");

    s_isNameRegistryInitialized = true;
    (void)GetNamePool(); // ensure name pool is initialized
}

void NameRegistry_Shutdown()
{
    HYP_CORE_ASSERT(s_isNameRegistryInitialized, "NameRegistry is not initialized");

    s_isNameRegistryInitialized = false;

    if (s_namePool != nullptr)
    {
        delete s_namePool;
        s_namePool = nullptr;
    }
}

#pragma endregion NameRegistry

#pragma region Name

NameRegistry* Name::s_registry = nullptr;

NameRegistry* Name::GetRegistry()
{
    static NameRegistry s_nameRegistry;

    return (s_registry = &s_nameRegistry);
}

Name Name::Unique(const ANSIStringView& prefix)
{
    return GetRegistry()->RegisterUniqueName(prefix, /* lock */ true);
}

const char* Name::LookupString() const
{
    return GetRegistry()->LookupStringForName(*this).Data();
}

String Name::ToString() const
{
    const char* str = LookupString();

    if (str && str[0] != '\0')
    {
        return String(str);
    }

    return String(HYP_FORMAT("Name({})", hashCode));
}

#pragma endregion Name

#pragma region WeakName

String WeakName::ToString() const
{
    return Name(hashCode).ToString();
}

#pragma endregion WeakName

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
