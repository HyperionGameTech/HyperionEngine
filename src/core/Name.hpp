#ifndef HYPERION_V2_CORE_NAME_HPP
#define HYPERION_V2_CORE_NAME_HPP

#include <Constants.hpp>
#include <core/Core.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/StaticString.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/HeapArray.hpp>
#include <core/lib/HashMap.hpp>

#include <mutex>

namespace hyperion {

class NameRegistry;

using NameID = UInt64;

struct Name
{
    static const Name invalid;

    static NameRegistry *GetRegistry();

    NameID hash_code;
    
    constexpr Name()
        : hash_code(0)
    {
    }

    constexpr Name(NameID id)
        : hash_code(id)
    {
    }

    constexpr Name(const Name &other) = default;
    constexpr Name &operator=(const Name &other) = default;
    constexpr Name(Name &&other) noexcept = default;
    constexpr Name &operator=(Name &&other) noexcept = default;

    constexpr NameID GetID() const
        { return hash_code; }

    constexpr bool IsValid() const
        { return hash_code != 0; }

    constexpr explicit operator Bool() const
        { return IsValid(); }

    constexpr explicit operator UInt64() const
        { return hash_code; }
    
    constexpr bool operator==(const Name &other) const
        { return hash_code == other.hash_code; }

    constexpr bool operator!=(const Name &other) const
        { return hash_code != other.hash_code; }

    constexpr bool operator<(const Name &other) const
        { return hash_code < other.hash_code; }

    constexpr bool operator<=(const Name &other) const
        { return hash_code <= other.hash_code; }

    constexpr bool operator>(const Name &other) const
        { return hash_code > other.hash_code; }

    constexpr bool operator>=(const Name &other) const
        { return hash_code >= other.hash_code; }

    const ANSIString &LookupString() const;

    constexpr HashCode GetHashCode() const
        { return HashCode(HashCode::ValueType(hash_code)); }
};

class NameRegistry
{
public:
    static constexpr SizeType num_name_groups = 256;

    NameRegistry() = default;
    NameRegistry(const NameRegistry &other) = delete;
    NameRegistry &operator=(const NameRegistry &other) = delete;
    NameRegistry(NameRegistry &&other) noexcept = delete;
    NameRegistry &operator=(NameRegistry &&other) noexcept = delete;
    ~NameRegistry() = default;

    Name RegisterName(NameID id, const ANSIString &str, bool lock)
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

    const ANSIString &LookupStringForName(Name name) const
    {
        if (!name.IsValid()) {
            return ANSIString::empty;
        }

        std::lock_guard guard(m_mutex);

        const auto it = m_name_map.Find(name.hash_code);

        if (it == m_name_map.End()) {
            return ANSIString::empty;
        }

        return it->value;
    }

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
        constexpr NameID id = hash_code.Value();// IsLittleEndian() ? hash_code.Value() : SwapEndianness(hash_code.Value());

        return id;
    }
    
    static NameID GenerateID(const ANSIString &str)
    {
        const HashCode hash_code = HashCode::GetHashCode(str.Data());
        const NameID id = hash_code.Value();// IsLittleEndian() ? hash_code.Value() : SwapEndianness(hash_code.Value());

        return id;
    }

    template <class HashedName>
    static NameRegistration FromHashedName(HashedName &&hashed_name, bool lock = true)
    {
        static constexpr NameID name_id = HashedName::hash_code.Value();

        Name::GetRegistry()->RegisterName(name_id, hashed_name.data, lock);

        return NameRegistration { name_id };
    }
    
    static NameRegistration FromDynamicString(const ANSIString &str)
    {
        const NameID id = GenerateID(str);

        Name::GetRegistry()->RegisterName(id, str, true);

        return NameRegistration { id };
    }
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

template <auto StaticStringType>
struct HashedName
{
    using Sequence = IntegerSequenceFromString<StaticStringType>;

    static constexpr HashCode hash_code = HashCode::GetHashCode(Sequence::Data());
    static constexpr const char *data   = Sequence::Data();
};

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