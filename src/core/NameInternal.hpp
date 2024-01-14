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

    constexpr Name(const Name &other)                   = default;
    constexpr Name &operator=(const Name &other)        = default;
    constexpr Name(Name &&other) noexcept               = default;
    constexpr Name &operator=(Name &&other) noexcept    = default;

    constexpr NameID GetID() const
        { return hash_code; }

    constexpr Bool IsValid() const
        { return hash_code != 0; }

    constexpr explicit operator Bool() const
        { return IsValid(); }

    constexpr explicit operator UInt64() const
        { return hash_code; }
    
    constexpr Bool operator==(const Name &other) const
        { return hash_code == other.hash_code; }

    constexpr Bool operator!=(const Name &other) const
        { return hash_code != other.hash_code; }

    constexpr Bool operator<(const Name &other) const
        { return hash_code < other.hash_code; }

    constexpr Bool operator<=(const Name &other) const
        { return hash_code <= other.hash_code; }

    constexpr Bool operator>(const Name &other) const
        { return hash_code > other.hash_code; }

    constexpr Bool operator>=(const Name &other) const
        { return hash_code >= other.hash_code; }

    const char *LookupString() const;

    constexpr HashCode GetHashCode() const
        { return HashCode(HashCode::ValueType(hash_code)); }

    static Name Unique();
    static Name Unique(const char *prefix);
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