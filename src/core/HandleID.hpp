#ifndef HYPERION_V2_CORE_HANDLE_ID_HPP
#define HYPERION_V2_CORE_HANDLE_ID_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/TypeID.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

namespace hyperion {

namespace v2 {

class Engine;

} // namespace v2

struct IDBase
{
    using ValueType = UInt;
    
    HYP_FORCE_INLINE explicit constexpr operator ValueType() const
        { return value; }
    
    HYP_FORCE_INLINE constexpr ValueType Value() const
        { return value; }
    
    HYP_FORCE_INLINE explicit constexpr operator bool() const
        { return bool(value); }

    HYP_FORCE_INLINE constexpr bool operator==(const IDBase &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE constexpr bool operator!=(const IDBase &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE constexpr bool operator<(const IDBase &other) const
        { return value < other.value; }

    /*! \brief If the value is non-zero, returns the ID minus one,
        to be used as a storage index. If the value is zero (invalid state),
        zero is returned. Ideally a validation check would be performed before you use this,
        unless you are totally sure that 0 is a valid index. */
    UInt ToIndex() const
        { return value ? value - 1 : 0; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);

        return hc;
    }

    ValueType value { 0 };
};

template <class T>
struct HandleID : IDBase
{
    constexpr HandleID() = default;

    explicit HandleID(const IDBase &other)
        : IDBase(other)
    {
    }

    constexpr HandleID(IDBase::ValueType value)
        : IDBase { value }
    {
    }

    constexpr HandleID(const HandleID &other)
        : IDBase(other)
    {
    }

    HandleID &operator=(const IDBase &other) = delete; // delete so we cannot assign a type's ID to a different type
    HandleID &operator=(const HandleID &other) = default;

    HYP_FORCE_INLINE operator bool() const
        { return IDBase::operator bool(); }

    HYP_FORCE_INLINE bool operator==(const HandleID &other) const
        { return IDBase::operator==(other); }

    HYP_FORCE_INLINE bool operator!=(const HandleID &other) const
        { return IDBase::operator!=(other); }

    HYP_FORCE_INLINE bool operator<(const HandleID &other) const
        { return IDBase::operator<(other); }

    const TypeID &GetTypeID() const
        { return TypeID::ForType<NormalizedType<T>>(); } 

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetTypeID().GetHashCode());
        hc.Add(IDBase::GetHashCode());

        return hc;
    }
};

} // namespace hyperion

#endif