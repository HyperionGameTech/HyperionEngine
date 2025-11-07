/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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
HYP_API extern Name CreateNameFromDynamicString(const ANSIString& str);

/*! \brief Creates a Name from a dynamic string. Does not add it to the registry. */
HYP_API extern StringHash CreateStringHashFromDynamicString(const ANSIStringView& str);

HYP_API extern Name RegisterName(NameRegistry* nameRegistry, NameID id, const ANSIString& str, bool lock);

HYP_API extern bool ShouldLockNameRegistry();

HYP_API extern void NameRegistry_Initialize();
HYP_API extern void NameRegistry_Shutdown();

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

    HYP_API static NameID GenerateID(const ANSIStringView& str);

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

    HYP_API static NameRegistration FromDynamicString(const ANSIString& str);
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

#if 0
// Name string literal conversion
Name operator "" _n(const char *, SizeType);

// Name (weak) string literal conversion
constexpr StringHash operator "" _nw(const char *, SizeType);
#endif

#if defined(HYP_COMPILE_TIME_NAME_HASHING) && HYP_COMPILE_TIME_NAME_HASHING

#define HYP_HASHED_NAME2(name) ::hyperion::HashedName<::hyperion::StaticString<sizeof(name)>(name)>()
#define HYP_NAME_UNSAFE2(name) ::hyperion::CreateNameFromStaticString_NoLock(HYP_HASHED_NAME2(name))
#define HYP_WEAK_NAME2(name) ::hyperion::Name(HYP_HASHED_NAME2(name).hashCode.Value())
#define HYP_NAME2(name) ::hyperion::CreateNameFromStaticString(HYP_HASHED_NAME2(name))

#define HYP_HASHED_NAME(name) ::hyperion::HashedName<::hyperion::StaticString<sizeof(HYP_STR(name))>(HYP_STR(name))>()
#define HYP_NAME_UNSAFE(name) ::hyperion::CreateNameFromStaticString_NoLock(HYP_HASHED_NAME(name))
#define HYP_WEAK_NAME(name) ::hyperion::Name(HYP_HASHED_NAME(name).hashCode.Value())
#define HYP_NAME(name) ::hyperion::CreateNameFromStaticString(HYP_HASHED_NAME(name))

#define NAME(str) HYP_NAME2(str)

#else
#define HYP_NAME(name) ::hyperion::Name(HashCode::GetHashCode(HYP_STR(name)).Value())
#define HYP_NAME_UNSAFE(name) HYP_NAME(name)
#define HYP_WEAK_NAME(name) HYP_NAME(name)

#define NAME(str) HYP_NAME(str)
#endif

#define NAME_FMT(fmt, ...) CreateNameFromDynamicString(HYP_FORMAT(fmt, ##__VA_ARGS__).Data())

} // namespace hyperion
