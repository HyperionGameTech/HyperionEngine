/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Map.hpp>
#include <Core/containers/String.hpp>
#include <Core/utilities/StringView.hpp>
#include <Core/utilities/FormatFwd.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/name/NameInternal.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

class NameRegistry;

/*! \brief Creates a Name from a dynamic string. Adds it to the registry, so a lock is required. */
CORE_API extern Name CreateNameFromDynamicString(const ANSIString& str);

/*! \brief Creates a Name from a dynamic string. Does not add it to the registry. */
CORE_API extern StringHash CreateStringHashFromDynamicString(const ANSIStringView& str);

CORE_API extern Name RegisterName(NameRegistry* nameRegistry, NameID id, const ANSIString& str, bool lock);
CORE_API extern bool ShouldLockNameRegistry();

void InitNameRegistry();
void DestroyNameRegistry();

struct NameRegistration
{
    NameID id;

    template <class S>
    static constexpr NameID GenerateID()
    {
        using Seq = typename S::Sequence;

        constexpr HashCode hashCode = HashCode::GetHashCode(Seq::Data());
        constexpr NameID id = hashCode.Value();

        return id;
    }

    CORE_API static NameID GenerateID(const ANSIStringView& str);

    template <class HashedName>
    static NameRegistration FromHashedName(HashedName&& hashedName, bool lock = true)
    {
        static constexpr NameID nameId = HashedName::hashCode.Value();

        RegisterName(Name::GetRegistry(), nameId, hashedName.data, lock);

        return NameRegistration { nameId };
    }

    template <HashCode::ValueType HashCode>
    static NameRegistration FromHashedName(const char* str, bool lock = true)
    {
        static constexpr NameID nameId = HashCode;

        RegisterName(Name::GetRegistry(), nameId, str, lock);

        return NameRegistration { nameId };
    }

    CORE_API static NameRegistration FromDynamicString(const ANSIString& str);
};

/*! \brief Creates a Name from a static string. The string must be a compile-time constant. */
template <class HashedName>
static inline Name CreateNameFromStaticString(HashedName&& hashedName)
{
    static const NameRegistration nameRegistration = NameRegistration::FromHashedName(std::forward<HashedName>(hashedName), ShouldLockNameRegistry());

    return Name(nameRegistration.id);
}

/*! \brief Creates a Name from a static string. The string must be a compile-time constant.
 * Use in contexts where thread-safety is not an issue such as static initialization. */
template <class HashedName>
static inline Name CreateNameFromStaticString_NoLock(HashedName&& hashedName)
{
    static const NameRegistration nameRegistration = NameRegistration::FromHashedName(std::forward<HashedName>(hashedName), false);

    return Name(nameRegistration.id);
}

// Formatter for Name
namespace utilities {

template <class StringType>
struct Formatter<StringType, Name>
{
    auto operator()(const Name& value) const
    {
        return StringType(value.LookupString());
    }
};

} // namespace utilities

// Name string literal conversion
Name operator""_name(const char*, size_t);

// StringHash literal conversion
HYP_CONSTEVAL StringHash operator""_sh(const char* str, size_t)
{
    return StringHash(str);
}

#if defined(HYP_COMPILE_TIME_NAME_HASHING) && HYP_COMPILE_TIME_NAME_HASHING

#define HYP_HASHED_NAME2(name) ::Hyperion::HashedName<::Hyperion::StaticString<sizeof(name)>(name)>()
#define HYP_NAME_UNSAFE2(name) ::Hyperion::CreateNameFromStaticString_NoLock(HYP_HASHED_NAME2(name))
#define HYP_WEAK_NAME2(name) ::Hyperion::Name(HYP_HASHED_NAME2(name).hashCode.Value())
#define HYP_NAME2(name) ::Hyperion::CreateNameFromStaticString(HYP_HASHED_NAME2(name))

#define HYP_HASHED_NAME(name) ::Hyperion::HashedName<::Hyperion::StaticString<sizeof(HYP_STR(name))>(HYP_STR(name))>()
#define HYP_NAME_UNSAFE(name) ::Hyperion::CreateNameFromStaticString_NoLock(HYP_HASHED_NAME(name))
#define HYP_WEAK_NAME(name) ::Hyperion::Name(HYP_HASHED_NAME(name).hashCode.Value())
#define HYP_NAME(name) ::Hyperion::CreateNameFromStaticString(HYP_HASHED_NAME(name))

#define NAME(str) HYP_NAME2(str)

#else
#define HYP_NAME(name) ::Hyperion::Name(HashCode::GetHashCode(HYP_STR(name)).Value())
#define HYP_NAME_UNSAFE(name) HYP_NAME(name)
#define HYP_WEAK_NAME(name) HYP_NAME(name)

#define NAME(str) HYP_NAME(str)
#endif

#define NAME_FMT(fmt, ...) CreateNameFromDynamicString(HYP_FORMAT(fmt, ##__VA_ARGS__).Data())

} // namespace Hyperion
