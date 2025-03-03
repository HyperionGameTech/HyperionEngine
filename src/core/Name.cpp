/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Name.hpp>

#include <core/utilities/UUID.hpp>
#include <core/utilities/Format.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

#pragma region NameRegistry

NameRegistry *g_name_registry = nullptr;

class NameRegistry
{
public:
    NameRegistry()                                          = default;
    NameRegistry(const NameRegistry &other)                 = delete;
    NameRegistry &operator=(const NameRegistry &other)      = delete;
    NameRegistry(NameRegistry &&other) noexcept             = delete;
    NameRegistry &operator=(NameRegistry &&other) noexcept  = delete;
    ~NameRegistry()                                         = default;

    Name RegisterName(NameID id, const ANSIString &str, bool lock);
    Name RegisterUniqueName(const ANSIString &str, bool lock);
    const ANSIString &LookupStringForName(Name name) const;

private:
    HashMap<NameID, Pair<ANSIString, uint32>>   m_name_map;
    mutable Mutex                               m_mutex;
};

Name NameRegistry::RegisterName(NameID id, const ANSIString &str, bool lock)
{
    Name name(id);

    if (lock) {
        m_mutex.Lock();
    }

    auto it = m_name_map.Find(id);

    if (it != m_name_map.End()) {
        Pair<ANSIString, uint32> &string_count_pair = it->second;

        ++string_count_pair.second;
    } else {
        m_name_map.Insert({ id, Pair<ANSIString, uint32> { str, 1 } });
    }

    if (lock) {
        m_mutex.Unlock();
    }

    return name;
}

Name NameRegistry::RegisterUniqueName(const ANSIString &str, bool lock)
{
    Name name;
    bool inserted = false;
    int suffix = 0;

    if (lock) {
        m_mutex.Lock();
    }

    while (!inserted) {
        ANSIString str_with_suffix = str;
    
        if (suffix > 0) {
            str_with_suffix = ANSIString(HYP_FORMAT("{}_{}", str, suffix));
        }

        const NameID name_id_with_suffix = NameRegistration::GenerateID(str_with_suffix);
    
        auto it = m_name_map.Find(name_id_with_suffix);

        if (it == m_name_map.End()) {
            m_name_map.Insert({
                name_id_with_suffix,
                Pair<ANSIString, uint32> { str_with_suffix, 1 }
            });

            name = Name(name_id_with_suffix);

            break;
        }

        ++suffix;
    }

    if (lock) {
        m_mutex.Unlock();
    }

    return name;
}

const ANSIString &NameRegistry::LookupStringForName(Name name) const
{
    if (!name.IsValid()) {
        return ANSIString::empty;
    }

    Mutex::Guard guard(m_mutex);

    const auto it = m_name_map.Find(name.hash_code);

    if (it == m_name_map.End()) {
        return ANSIString::empty;
    }

    return it->second.first;
}

HYP_API Name RegisterName(NameRegistry *name_registry, NameID id, const ANSIString &str, bool lock)
{
    return name_registry->RegisterName(id, str, lock);
}

HYP_API const ANSIString &LookupStringForName(const NameRegistry *name_registry, Name name)
{
    return name_registry->LookupStringForName(name);
}

#pragma endregion NameRegistry

#pragma region Name

NameRegistry *Name::GetRegistry()
{
    static struct NameRegistryInitializer
    {
        NameRegistryInitializer()
        {
            if (!g_name_registry)
            {
                g_name_registry = new NameRegistry();
            }
        }
    } initializer;

    return g_name_registry;
}

Name Name::Unique(const char *prefix)
{
    return GetRegistry()->RegisterUniqueName(ANSIString(prefix), /* lock */ true);
}

const char *Name::LookupString() const
{
    return GetRegistry()->LookupStringForName(*this).Data();
}

#pragma endregion Name

Name CreateNameFromDynamicString(const ANSIString &str)
{
    const NameRegistration name_registration = NameRegistration::FromDynamicString(str);

    return Name(name_registration.id);
}

WeakName CreateWeakNameFromDynamicString(const ANSIStringView &str)
{
    return WeakName(NameRegistration::GenerateID(str));
}

#pragma region NameRegistration

NameID NameRegistration::GenerateID(const ANSIStringView &str)
{
    const HashCode hash_code = HashCode::GetHashCode(str.Data());
    const NameID id = hash_code.Value();

    return id;
}

NameRegistration NameRegistration::FromDynamicString(const ANSIString &str)
{
    const NameID id = GenerateID(str);

    Name::GetRegistry()->RegisterName(id, str, true);

    return NameRegistration { id };
}

#pragma endregion NameRegistration

} // namespace hyperion