/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Name.hpp>
#include <core/utilities/UUID.hpp>

#include <math/MathUtil.hpp>

namespace hyperion {

NameRegistry *g_name_registry = nullptr;

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

Name Name::Unique()
{
    return CreateNameFromDynamicString(UUID().ToString());
}

Name Name::Unique(const char *prefix)
{
    return CreateNameFromDynamicString(ANSIString(prefix) + "_" + UUID().ToString());
}

const char *Name::LookupString() const
{
    return GetRegistry()->LookupStringForName(*this).Data();
}

Name CreateNameFromDynamicString(const ANSIString &str)
{
    const NameRegistration name_registration = NameRegistration::FromDynamicString(str);

    return Name(name_registration.id);
}

WeakName CreateWeakNameFromDynamicString(const ANSIStringView &str)
{
    return WeakName(NameRegistration::GenerateID(str));
}

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

Name NameRegistry::RegisterName(NameID id, const ANSIString &str, bool lock)
{
    Name name(id);

    if (lock) {
        m_mutex.Lock();
    }

    m_name_map.Set(id, str);

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

    return it->second;
}

} // namespace hyperion