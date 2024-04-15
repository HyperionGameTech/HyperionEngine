/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_CORE_NAME_INTERNAL_HPP
#define HYPERION_V2_CORE_NAME_INTERNAL_HPP

#include <Constants.hpp>
#include <core/Core.hpp>
#include <core/lib/StaticString.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/HeapArray.hpp>
#include <core/lib/HashMap.hpp>
#include <HashCode.hpp>

#include <mutex>

namespace hyperion {

class NameRegistry;

using NameID = uint64;

struct Name
{
    NameID hash_code;
    
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

    constexpr HashCode GetHashCode() const
        { return HashCode(HashCode::ValueType(hash_code)); }

    HYP_API const char *LookupString() const;

    HYP_API static NameRegistry *GetRegistry();

    HYP_API static Name Unique();
    HYP_API static Name Unique(const char *prefix);

    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr Name Invalid()
        { return Name { 0 }; };
};

template <auto StaticStringType>
struct HashedName
{
    using Sequence = IntegerSequenceFromString<StaticStringType>;

    static constexpr HashCode hash_code = HashCode::GetHashCode(Sequence::Data());
    static constexpr const char *data   = Sequence::Data();
};

} // namespace hyperion

#endif