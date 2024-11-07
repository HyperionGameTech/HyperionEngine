/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_ID_HPP
#define HYPERION_CORE_ID_HPP

#include <core/utilities/TypeID.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

namespace hyperion {

class Engine;

struct IDBase
{
    using ValueType = uint32;

    HYP_FORCE_INLINE constexpr bool IsValid() const
        { return bool(value); }
    
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
    HYP_FORCE_INLINE uint32 ToIndex(uint32 invalid_value = 0) const
        { return value ? value - 1 : invalid_value; }

    ValueType value { 0 };
};

template <class T>
struct ID;

template <class T>
struct ID : IDBase
{
    static const ID invalid;

    constexpr ID() = default;

    explicit ID(const IDBase &other)
        : IDBase(other)
    {
    }

    constexpr ID(IDBase::ValueType value)
        : IDBase { value }
    {
    }

    constexpr ID(const ID &other)
        : IDBase(other)
    {
    }

    ID &operator=(const IDBase &other) = delete; // delete so we cannot assign a type's ID to a different type
    ID &operator=(const ID &other) = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return IDBase::operator bool(); }

    HYP_FORCE_INLINE bool operator==(const ID &other) const
        { return IDBase::operator==(other); }

    HYP_FORCE_INLINE bool operator!=(const ID &other) const
        { return IDBase::operator!=(other); }

    HYP_FORCE_INLINE bool operator<(const ID &other) const
        { return IDBase::operator<(other); }

    TypeID GetTypeID() const
        { return TypeID::ForType<NormalizedType<T>>(); }
    
    static Name GetTypeName()
    {
        static const Name type_name = CreateNameFromDynamicString(TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        return type_name;
    }

    static ID FromIndex(uint32 index)
    {
        ID id;
        id.value = index + 1;
        return id;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetTypeName().GetHashCode());
        hc.Add(value);

        return hc;
    }
};

template <class T>
const ID<T> ID<T>::invalid = ID<T>();

} // namespace hyperion

#endif