#include <core/Name.hpp>
#include <math/MathUtil.hpp>

namespace hyperion {

static ANSIString GenerateUUID()
{
    static constexpr SizeType num_uuid_groups = 5;
    static constexpr SizeType num_uuid_chars = 4;

    static constexpr char uuid_chars[] = "0123456789abcdef";

    ANSIString uuid;
    uuid.Reserve(num_uuid_groups * num_uuid_chars + num_uuid_groups - 1);

    for (SizeType i = 0; i < num_uuid_groups; ++i) {
        for (SizeType j = 0; j < num_uuid_chars; ++j) {
            const SizeType index = MathUtil::RandRange(0ull, sizeof(uuid_chars) - 1ull);

            uuid.Append(index);
        }

        if (i < num_uuid_groups - 1) {
            uuid.Append('-');
        }
    }

    return uuid;
}

const Name Name::invalid = Name(0);

NameRegistry *Name::GetRegistry()
{
    static NameRegistry registry;

    return &registry;
}

Name Name::Unique()
{
    return CreateNameFromDynamicString(GenerateUUID());
}

Name Name::Unique(const char *prefix)
{
    return CreateNameFromDynamicString(ANSIString(prefix) + "_" + GenerateUUID());
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