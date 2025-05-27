/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_NAME_HPP
#define HYPERION_CORE_NAME_HPP

#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/FormatFwd.hpp>
#include <core/threading/Mutex.hpp>

#include <core/NameInternal.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class NameRegistry;

/*! \brief Creates a Name from a dynamic string. Adds it to the registry, so a lock is required. */
extern HYP_API Name CreateNameFromDynamicString(const ANSIString& str);

/*! \brief Creates a Name from a dynamic string. Does not add it to the registry. */
extern HYP_API WeakName CreateWeakNameFromDynamicString(const ANSIStringView& str);

extern HYP_API Name RegisterName(NameRegistry* name_registry, NameID id, const ANSIString& str, bool lock);

extern HYP_API const ANSIString& LookupStringForName(const NameRegistry* name_registry, Name name);

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

    HYP_API static NameID GenerateID(const ANSIStringView& str);

    template <class HashedName>
    static NameRegistration FromHashedName(HashedName&& hashed_name, bool lock = true)
    {
        static constexpr NameID name_id = HashedName::hash_code.Value();

        RegisterName(Name::GetRegistry(), name_id, hashed_name.data, lock);

        return NameRegistration { name_id };
    }

    template <HashCode::ValueType HashCode>
    static NameRegistration FromHashedName(const char* str, bool lock = true)
    {
        static constexpr NameID name_id = HashCode;

        RegisterName(Name::GetRegistry(), name_id, str, lock);

        return NameRegistration { name_id };
    }

    HYP_API static NameRegistration FromDynamicString(const ANSIString& str);
};

/*! \brief Creates a Name from a static string. The string must be a compile-time constant. */
template <class HashedName>
static inline Name CreateNameFromStaticString_WithLock(HashedName&& hashed_name)
{
    static const NameRegistration name_registration = NameRegistration::FromHashedName(std::forward<HashedName>(hashed_name), true);

    return Name(name_registration.id);
}

/*! \brief Creates a Name from a static string. The string must be a compile-time constant.
 * Use in contexts where thread-safety is not an issue such as static initialization. */
template <class HashedName>
static inline Name CreateNameFromStaticString_NoLock(HashedName&& hashed_name)
{
    static const NameRegistration name_registration = NameRegistration::FromHashedName(std::forward<HashedName>(hashed_name), false);

    return Name(name_registration.id);
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

#if 0
// Name string literal conversion
Name operator "" _n(const char *, SizeType);

// Name (weak) string literal conversion
constexpr WeakName operator "" _nw(const char *, SizeType);
#endif

#if defined(HYP_COMPILE_TIME_NAME_HASHING) && HYP_COMPILE_TIME_NAME_HASHING

    #define HYP_HASHED_NAME2(name) ::hyperion::HashedName<::hyperion::StaticString<sizeof(name)>(name)>()
    #define HYP_NAME_UNSAFE2(name) ::hyperion::CreateNameFromStaticString_NoLock(HYP_HASHED_NAME2(name))
    #define HYP_WEAK_NAME2(name) ::hyperion::Name(HYP_HASHED_NAME2(name).hash_code.Value())
    #define HYP_NAME2(name) ::hyperion::CreateNameFromStaticString_WithLock(HYP_HASHED_NAME2(name))

    #define HYP_HASHED_NAME(name) ::hyperion::HashedName<::hyperion::StaticString<sizeof(HYP_STR(name))>(HYP_STR(name))>()
    #define HYP_NAME_UNSAFE(name) ::hyperion::CreateNameFromStaticString_NoLock(HYP_HASHED_NAME(name))
    #define HYP_WEAK_NAME(name) ::hyperion::Name(HYP_HASHED_NAME(name).hash_code.Value())
    #define HYP_NAME(name) ::hyperion::CreateNameFromStaticString_WithLock(HYP_HASHED_NAME(name))

    #define NAME(str) HYP_NAME2(str)

#else
    #define HYP_NAME(name) ::hyperion::Name(HashCode::GetHashCode(HYP_STR(name)).Value())
    #define HYP_NAME_UNSAFE(name) HYP_NAME(name)
    #define HYP_WEAK_NAME(name) HYP_NAME(name)

    #define NAME(str) HYP_NAME(str)
#endif

#define NAME_FMT(fmt, ...) CreateNameFromDynamicString(HYP_FORMAT(fmt, ##__VA_ARGS__).Data())

} // namespace hyperion

#endif