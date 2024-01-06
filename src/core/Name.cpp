#include <core/Name.hpp>

namespace hyperion {

const Name Name::invalid = Name(0);

NameRegistry *Name::GetRegistry()
{
    static NameRegistry registry;

    return &registry;
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

NameID NameRegistration::GenerateID(const ANSIString &str)
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
        m_mutex.lock();
    }

    m_name_map.Set(id, str);

    if (lock) {
        m_mutex.unlock();
    }

    return name;
}

const ANSIString &NameRegistry::LookupStringForName(Name name) const
{
    if (!name.IsValid()) {
        return ANSIString::empty;
    }

    std::lock_guard guard(m_mutex);

    const auto it = m_name_map.Find(name.hash_code);

    if (it == m_name_map.End()) {
        return ANSIString::empty;
    }

    return it->second;
}

} // namespace hyperion