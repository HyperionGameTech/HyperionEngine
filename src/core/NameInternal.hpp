/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_NAME_INTERNAL_HPP
#define HYPERION_CORE_NAME_INTERNAL_HPP

#include <core/containers/StaticString.hpp>
#include <core/containers/HeapArray.hpp>
#include <core/Defines.hpp>

#include <HashCode.hpp>

namespace hyperion {

class NameRegistry;

using NameID = uint64;

extern NameRegistry *g_name_registry;

struct Name;
struct WeakName;

template <auto StaticStringType>
struct HashedName;

template <HashCode::ValueType HashCode>
static inline Name CreateNameFromStaticString_WithLock(const char *str);

extern HYP_API Name CreateNameFromDynamicString(const ANSIString &str);

namespace detail {

template <class StringType>
constexpr Name MakeName_Impl(StringType str);

} // namespace detail

/*! \brief A name is a hashed string that is used to identify objects, components, and other entities in the engine.
 *  \details Names have their text components stored in a global registry and are internally.
 *  A \ref{Name} holds a 64 bit unsigned integer representing the hash, allowing for fast lookups and comparisons.
 *
 *  To create a name at compile time, use the \ref{HYP_NAME} macro.
 *  \code{.cpp}
 *  Name name = NAME("MyName");
 *  \endcode
 *
 *  To create a name at runtime, use the \ref{CreateNameFromDynamicString} function.
 *  \code{.cpp}
 *  Name name = CreateNameFromDynamicString("MyName");
 *  \endcode
 */
struct Name
{
    friend constexpr bool operator==(const Name &lhs, const Name &rhs);
    friend constexpr bool operator==(const Name &lhs, const WeakName &rhs);
    friend constexpr bool operator!=(const Name &lhs, const Name &rhs);
    friend constexpr bool operator!=(const Name &lhs, const WeakName &rhs);
    friend constexpr bool operator<(const Name &lhs, const Name &rhs);
    friend constexpr bool operator<(const Name &lhs, const WeakName &rhs);
    friend constexpr bool operator<=(const Name &lhs, const Name &rhs);
    friend constexpr bool operator<=(const Name &lhs, const WeakName &rhs);
    friend constexpr bool operator>(const Name &lhs, const Name &rhs);
    friend constexpr bool operator>(const Name &lhs, const WeakName &rhs);
    friend constexpr bool operator>=(const Name &lhs, const Name &rhs);
    friend constexpr bool operator>=(const Name &lhs, const WeakName &rhs);

    HashCode::ValueType hash_code;
    
    constexpr Name()
        : hash_code(0)
    {
    }

    constexpr Name(NameID id)
        : hash_code(id)
    {
    }

    constexpr Name(const Name &other)                   = default;
    constexpr Name &operator=(const Name &other)        = default;
    constexpr Name(Name &&other) noexcept               = default;
    constexpr Name &operator=(Name &&other) noexcept    = default;

    constexpr NameID GetID() const
        { return hash_code; }

    constexpr bool IsValid() const
        { return hash_code != 0; }

    constexpr explicit operator bool() const
        { return IsValid(); }

    constexpr explicit operator uint64() const
        { return hash_code; }

    /*! \brief For convenience, operator* is overloaded to return the string representation of the name,
     *  if it is found in the name registry. Otherwise, it returns an empty string. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    const char *operator*() const
        { return LookupString(); }

    constexpr HashCode GetHashCode() const
        { return HashCode(HashCode::ValueType(hash_code)); }

    /*! \brief Returns the string representation of the name, if it is found in the name registry.
     *  Otherwise, it returns an empty string. */
    [[nodiscard]]
    HYP_API const char *LookupString() const;

    HYP_API static NameRegistry *GetRegistry();

    /*! \brief Generates a unique name via UUID. */
    HYP_API static Name Unique();
    /*! \brief Generates a unique name with a prefix. */
    HYP_API static Name Unique(const char *prefix);

    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr Name Invalid()
        { return Name { 0 }; };

    // template <class StaticStringType>
    // [[nodiscard]]
    // HYP_FORCE_INLINE
    // static constexpr Name FromStaticString(StaticStringType static_string)
    //     { return Name { (CreateNameFromStaticString_WithLock<HashCode::GetHashCode(StaticString)>(StaticString.data).hash_code) }; }
};

struct WeakName
{
    friend constexpr bool operator==(const WeakName &lhs, const WeakName &rhs);
    friend constexpr bool operator==(const WeakName &lhs, const Name &rhs);
    friend constexpr bool operator!=(const WeakName &lhs, const WeakName &rhs);
    friend constexpr bool operator!=(const WeakName &lhs, const Name &rhs);
    friend constexpr bool operator<(const WeakName &lhs, const WeakName &rhs);
    friend constexpr bool operator<(const WeakName &lhs, const Name &rhs);
    friend constexpr bool operator<=(const WeakName &lhs, const WeakName &rhs);
    friend constexpr bool operator<=(const WeakName &lhs, const Name &rhs);
    friend constexpr bool operator>(const WeakName &lhs, const WeakName &rhs);
    friend constexpr bool operator>(const WeakName &lhs, const Name &rhs);
    friend constexpr bool operator>=(const WeakName &lhs, const WeakName &rhs);
    friend constexpr bool operator>=(const WeakName &lhs, const Name &rhs);

    HashCode::ValueType hash_code;
    
    constexpr WeakName()
        : hash_code(0)
    {
    }

    constexpr WeakName(NameID id)
        : hash_code(id)
    {
    }
    
    constexpr WeakName(const Name &name)
        : hash_code(name.hash_code)
    {
    }

    constexpr WeakName &operator=(const Name &name)
    {
        hash_code = name.hash_code;

        return *this;
    }

    constexpr WeakName(const WeakName &other)                   = default;
    constexpr WeakName &operator=(const WeakName &other)        = default;
    constexpr WeakName(WeakName &&other) noexcept               = default;
    constexpr WeakName &operator=(WeakName &&other) noexcept    = default;

    constexpr NameID GetID() const
        { return hash_code; }

    constexpr bool IsValid() const
        { return hash_code != 0; }

    constexpr explicit operator bool() const
        { return IsValid(); }

    constexpr explicit operator uint64() const
        { return hash_code; }

    constexpr HashCode GetHashCode() const
        { return HashCode(HashCode::ValueType(hash_code)); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr WeakName Invalid()
        { return WeakName { 0 }; };
};

constexpr bool operator==(const Name &lhs, const Name &rhs)
{
    return lhs.hash_code == rhs.hash_code;
}

constexpr bool operator==(const Name &lhs, const WeakName &rhs)
{
    return lhs.hash_code == rhs.hash_code;
}

constexpr bool operator!=(const Name &lhs, const Name &rhs)
{
    return lhs.hash_code != rhs.hash_code;
}

constexpr bool operator!=(const Name &lhs, const WeakName &rhs)
{
    return lhs.hash_code != rhs.hash_code;
}

constexpr bool operator<(const Name &lhs, const Name &rhs)
{
    return lhs.hash_code < rhs.hash_code;
}

constexpr bool operator<(const Name &lhs, const WeakName &rhs)
{
    return lhs.hash_code < rhs.hash_code;
}

constexpr bool operator<=(const Name &lhs, const Name &rhs)
{
    return lhs.hash_code <= rhs.hash_code;
}

constexpr bool operator<=(const Name &lhs, const WeakName &rhs)
{
    return lhs.hash_code <= rhs.hash_code;
}

constexpr bool operator>(const Name &lhs, const Name &rhs)
{
    return lhs.hash_code > rhs.hash_code;
}

constexpr bool operator>(const Name &lhs, const WeakName &rhs)
{
    return lhs.hash_code > rhs.hash_code;
}

constexpr bool operator>=(const Name &lhs, const Name &rhs)
{
    return lhs.hash_code >= rhs.hash_code;
}

constexpr bool operator>=(const Name &lhs, const WeakName &rhs)
{
    return lhs.hash_code >= rhs.hash_code;
}

constexpr bool operator==(const WeakName &lhs, const WeakName &rhs)
{
    return lhs.hash_code == rhs.hash_code;
}

constexpr bool operator==(const WeakName &lhs, const Name &rhs)
{
    return lhs.hash_code == rhs.hash_code;
}

constexpr bool operator!=(const WeakName &lhs, const WeakName &rhs)
{
    return lhs.hash_code != rhs.hash_code;
}

constexpr bool operator!=(const WeakName &lhs, const Name &rhs)
{
    return lhs.hash_code != rhs.hash_code;
}

constexpr bool operator<(const WeakName &lhs, const WeakName &rhs)
{
    return lhs.hash_code < rhs.hash_code;
}

constexpr bool operator<(const WeakName &lhs, const Name &rhs)
{
    return lhs.hash_code < rhs.hash_code;
}

constexpr bool operator<=(const WeakName &lhs, const WeakName &rhs)
{
    return lhs.hash_code <= rhs.hash_code;
}

constexpr bool operator<=(const WeakName &lhs, const Name &rhs)
{
    return lhs.hash_code <= rhs.hash_code;
}

constexpr bool operator>(const WeakName &lhs, const WeakName &rhs)
{
    return lhs.hash_code > rhs.hash_code;
}

constexpr bool operator>(const WeakName &lhs, const Name &rhs)
{
    return lhs.hash_code > rhs.hash_code;
}

constexpr bool operator>=(const WeakName &lhs, const WeakName &rhs)
{
    return lhs.hash_code >= rhs.hash_code;
}

constexpr bool operator>=(const WeakName &lhs, const Name &rhs)
{
    return lhs.hash_code >= rhs.hash_code;
}

template <auto StaticStringType>
struct HashedName
{
    using Sequence = containers::detail::IntegerSequenceFromString<StaticStringType>;

    static constexpr HashCode hash_code = HashCode::GetHashCode(Sequence::Data());
    static constexpr const char *data = Sequence::Data();
};

namespace detail {

template <class StringType>
constexpr Name MakeName_Impl(StringType str_arg)
{
    constexpr auto str = HYP_GET_CONST_ARG(str_arg);

    return Name { CreateNameFromStaticString_WithLock<HashCode::GetHashCode(str)>(str.data).hash_code };
}

// template <class StringType>
// constexpr auto MakeStaticString_Impl(StringType str_arg)
// {
//     return StaticString(HYP_GET_CONST_ARG(str_arg));
// }

} // namespace detail

} // namespace hyperion

#endif