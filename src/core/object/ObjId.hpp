/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_ID_HPP
#define HYPERION_CORE_ID_HPP

#include <core/utilities/TypeId.hpp>
#include <core/utilities/UniqueID.hpp>

#include <core/utilities/FormatFwd.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

namespace hyperion {

HYP_API extern ANSIStringView GetClassName(const TypeId& type_id);

struct ObjIdBase
{
    constexpr ObjIdBase() = default;

    constexpr ObjIdBase(TypeId type_id, uint32 value)
        : type_id_value(type_id.Value()),
          value(value)
    {
    }

    ObjIdBase(uint32 value) = delete; // delete so we cannot create an ObjIdBase with just a value, it must have a type_id

    constexpr ObjIdBase(const ObjIdBase& other) = default;
    ObjIdBase& operator=(const ObjIdBase& other) = default;

    constexpr ObjIdBase(ObjIdBase&& other) noexcept = default;
    ObjIdBase& operator=(ObjIdBase&& other) noexcept = default;

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return type_id_value != 0 && value != 0;
    }

    HYP_FORCE_INLINE constexpr uint32 Value() const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr TypeId GetTypeId() const
    {
        return TypeId { type_id_value };
    }

    HYP_FORCE_INLINE explicit constexpr operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE constexpr bool operator==(const ObjIdBase& other) const
    {
        return type_id_value == other.type_id_value && value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const ObjIdBase& other) const
    {
        return type_id_value != other.type_id_value || value != other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const ObjIdBase& other) const
    {
        if (type_id_value != other.type_id_value)
        {
            return type_id_value < other.type_id_value;
        }

        return value < other.value;
    }

    /*! \brief If the value is non-zero, returns the ObjId minus one,
        to be used as a storage index. If the value is zero (invalid state),
        zero is returned. Ideally a validation check would be performed before you use this,
        unless you are totally sure that 0 is a valid index. */
    HYP_FORCE_INLINE constexpr uint32 ToIndex(uint32 invalid_value = 0) const
    {
        return value ? value - 1 : invalid_value;
    }

    HYP_FORCE_INLINE operator UniqueID() const
    {
        return UniqueID { GetHashCode() };
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type_id_value);
        hc.Add(value);

        return hc;
    }

    uint32 type_id_value = 0;
    uint32 value = 0;
};

/*! \brief A transient, global identifier for an instance of an object. Object is not guaranteed to be alive when this ObjId is used.
 *  \details The object this is referencing may not be of type T as it may be a subclass of T. Use TypeId() to get the runtime type ObjId of the object.
 *  \warning This identifier is NOT guaranteed to be stable across runs of the engine. Do not use it for persistent storage or serialization. */
template <class T>
struct ObjId : ObjIdBase
{
    using ObjectType = T;

    static const ObjId invalid;
    static constexpr TypeId type_id_static = TypeId::ForType<T>();

    constexpr ObjId()
        : ObjIdBase { type_id_static, 0 }
    {
    }

    explicit constexpr ObjId(const ObjIdBase& other)
        : ObjIdBase(other)
    {
    }

    ObjId& operator=(const ObjIdBase& other) = delete; // delete so we cannot assign a type's ObjId to a different type

    constexpr ObjId(const ObjId& other) = default;
    ObjId& operator=(const ObjId& other) = default;

    constexpr ObjId(ObjId&& other) noexcept = default;
    ObjId& operator=(ObjId&& other) noexcept = default;

    static ObjId FromIndex(uint32 index)
    {
        return ObjId { ObjIdBase { type_id_static, index + 1 } };
    }

    /*! \brief Allows implicit conversion to ObjId<Ty> where T is convertible to Ty.
     *  \details This is useful for converting IDs of derived types to IDs of base types. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator ObjId<Ty>() const
    {
        return ObjId<Ty>(static_cast<const ObjIdBase&>(*this));
    }

    /*! \brief Allows explicit conversion to ObjId<Ty> where T is a base class of Ty.
     *  \details This is useful for converting IDs of base types to IDs of derived types, but requires an explicit cast to avoid accidental conversions. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator ObjId<Ty>() const
    {
        if (!IsValid())
        {
            return ObjId<Ty>::invalid;
        }

        /// \todo Add a AssertDebug here to ensure that type_id is in the hierarchy of Ty

        return ObjId<Ty>(static_cast<const ObjIdBase&>(*this));
    }
};

template <class T>
const ObjId<T> ObjId<T>::invalid = ObjId<T>();

// string format specialization for ObjId<T>
namespace utilities {

template <class StringType, class T>
struct Formatter<StringType, ObjId<T>>
{
    auto operator()(const ObjId<T>& value) const
    {
        return Formatter<StringType, ANSIStringView> {}(GetClassName(value.GetTypeId())) + StringType("#") + Formatter<StringType, uint32> {}(value.Value());
    }
};

template <class StringType>
struct Formatter<StringType, ObjIdBase>
{
    auto operator()(const ObjIdBase& value) const
    {
        return Formatter<StringType, ANSIStringView> {}(GetClassName(value.GetTypeId())) + StringType("#") + Formatter<StringType, uint32> {}(value.Value());
    }
};

} // namespace utilities

} // namespace hyperion

#endif