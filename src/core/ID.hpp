#ifndef HYPERION_V2_CORE_ID_HPP
#define HYPERION_V2_CORE_ID_HPP

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
    HYP_FORCE_INLINE UInt ToIndex() const
        { return value ? value - 1 : 0; }

    ValueType value { 0 };
};

template <class T>
struct ID;

struct EncodedID
{
    TypeID type_id;
    IDBase::ValueType value;

    EncodedID()
        : type_id(TypeID::ForType<void>()),
          value(0)
    {
    }

    template <class T>
    EncodedID(ID<T> id)
        : type_id(TypeID::ForType<NormalizedType<T>>()),
          value(id.value)
    {
    }

    EncodedID(const EncodedID &other) = default;
    EncodedID &operator=(const EncodedID &other) = default;

    EncodedID(EncodedID &&other) noexcept = default;
    EncodedID &operator=(EncodedID &&other) noexcept = default;

    HYP_FORCE_INLINE bool IsValid() const
        { return type_id != TypeID::ForType<void>() && value != 0; }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    template <class T>
    HYP_FORCE_INLINE bool operator==(const ID<T> &other) const
        { return type_id == TypeID::ForType<NormalizedType<T>>() && value == other.value; }

    template <class T>
    HYP_FORCE_INLINE bool operator!=(const ID<T> &other) const
        { return type_id != TypeID::ForType<NormalizedType<T>>() || value != other.value; }

    ~EncodedID() = default;

    template <class T>
    ID<T> Decode() const
    {
        if (type_id != TypeID::ForType<NormalizedType<T>>()) {
            return ID<NormalizedType<T>>();
        }

        return ID<NormalizedType<T>>(value);
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type_id);
        hc.Add(value);

        return hc;
    }
};

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

    HYP_FORCE_INLINE bool operator==(const EncodedID &other) const
        { return other.type_id == GetTypeID() && other.value == value; }

    HYP_FORCE_INLINE bool operator!=(const ID &other) const
        { return IDBase::operator!=(other); }

    HYP_FORCE_INLINE bool operator!=(const EncodedID &other) const
        { return other.type_id != GetTypeID() || other.value != value; }

    HYP_FORCE_INLINE bool operator<(const ID &other) const
        { return IDBase::operator<(other); }

    TypeID GetTypeID() const
        { return TypeID::ForType<NormalizedType<T>>(); }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetTypeID().GetHashCode());
        hc.Add(value);

        return hc;
    }
};

template <class T>
const ID<T> ID<T>::invalid = ID<T>();

} // namespace hyperion

#endif