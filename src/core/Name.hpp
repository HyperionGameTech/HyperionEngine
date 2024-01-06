#ifndef HYPERION_V2_CORE_NAME_HPP
#define HYPERION_V2_CORE_NAME_HPP

#include <core/lib/HashMap.hpp>
#include <core/lib/String.hpp>

#include <core/NameInternal.hpp>

namespace hyperion {

class NameRegistry
{
public:
    static constexpr SizeType num_name_groups = 256;

    NameRegistry()                                          = default;
    NameRegistry(const NameRegistry &other)                 = delete;
    NameRegistry &operator=(const NameRegistry &other)      = delete;
    NameRegistry(NameRegistry &&other) noexcept             = delete;
    NameRegistry &operator=(NameRegistry &&other) noexcept  = delete;
    ~NameRegistry()                                         = default;

    Name RegisterName(NameID id, const ANSIString &str, bool lock);
    const ANSIString &LookupStringForName(Name name) const;

private:
    HashMap<NameID, ANSIString> m_name_map;
    mutable std::mutex          m_mutex;
};


struct NameRegistration
{
    NameID id;
    
    template <class S>
    static constexpr NameID GenerateID()
    {
        using Seq = typename S::Sequence;
        
        constexpr HashCode hash_code = HashCode::GetHashCode(Seq::Data());
        constexpr NameID id = hash_code.Value();

        return id;
    }
    
    static NameID GenerateID(const ANSIString &str);

    template <class HashedName>
    static NameRegistration FromHashedName(HashedName &&hashed_name, Bool lock = true)
    {
        static constexpr NameID name_id = HashedName::hash_code.Value();

        Name::GetRegistry()->RegisterName(name_id, hashed_name.data, lock);

        return NameRegistration { name_id };
    }
    
    static NameRegistration FromDynamicString(const ANSIString &str);
};

/**
 * \brief Creates a Name from a static string. The string must be a compile-time constant.
 */
template <class HashedName>
static inline Name CreateNameFromStaticString_WithLock(HashedName &&hashed_name)
{
    static const NameRegistration name_registration = NameRegistration::FromHashedName(std::forward<HashedName>(hashed_name), true);

    return Name(name_registration.id);
}

/**
 * \brief Creates a Name from a static string. The string must be a compile-time constant.
 * Use in contexts where thread-safety is not an issue such as static initialization.
 */
template <class HashedName>
static inline Name CreateNameFromStaticString_NoLock(HashedName &&hashed_name)
{
    static const NameRegistration name_registration = NameRegistration::FromHashedName(std::forward<HashedName>(hashed_name), false);

    return Name(name_registration.id);
}


/**
 * \brief Creates a Name from a dynamic string.
 */
Name CreateNameFromDynamicString(const ANSIString &str);

#if defined(HYP_COMPILE_TIME_NAME_HASHING) && HYP_COMPILE_TIME_NAME_HASHING
#define HYP_HASHED_NAME(name)   ::hyperion::HashedName<::hyperion::StaticString<sizeof(HYP_STR(name))>(HYP_STR(name))>()
#define HYP_NAME(name)          ::hyperion::CreateNameFromStaticString_WithLock(HYP_HASHED_NAME(name))
#define HYP_NAME_UNSAFE(name)   ::hyperion::CreateNameFromStaticString_NoLock(HYP_HASHED_NAME(name))
#define HYP_WEAK_NAME(name)     ::hyperion::Name(HYP_HASHED_NAME(name).hash_code.Value())
#else
#define HYP_NAME(name)          ::hyperion::Name(HashCode::GetHashCode(HYP_STR(name)).Value())
#define HYP_NAME_UNSAFE(name)   HYP_NAME(name)
#define HYP_WEAK_NAME(name)     HYP_NAME(name)
#endif

} // namespace hyperion

#endif