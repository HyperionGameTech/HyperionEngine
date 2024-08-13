/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_PROPERTY_HPP
#define HYPERION_CORE_HYP_PROPERTY_HPP

#include <core/object/HypPropertySerializer.hpp>
#include <core/object/HypData.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/Serialization.hpp>
#include <asset/serialization/SerializationWrapper.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

struct HypPropertyTypeInfo
{
    TypeID  target_type_id;
    TypeID  value_type_id; // for getter or setter: getter is param type, setter is return type
};

namespace detail {

template <class T>
using UnwrappedSerializationType = NormalizedType<typename SerializationWrapperReverseMapping<NormalizedType<T>>::Type>;

template <class T>
constexpr TypeID GetUnwrappedSerializationTypeID()
{
    return TypeID::ForType<UnwrappedSerializationType<T>>();
}

} // namespace detail

struct HypPropertyGetter
{
    Proc<HypData, const void *> GetterForTargetPointer;
    HypPropertyTypeInfo         type_info;

    HypPropertyGetter() = default;

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType(TargetType::*MemFn)())
        : GetterForTargetPointer([MemFn](const void *target) -> HypData
          {
              return HypData((static_cast<TargetType *>(target)->*MemFn)());
          })

    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType(TargetType::*MemFn)() const)
        : GetterForTargetPointer([MemFn](const void *target) -> HypData
          {
              return HypData((static_cast<const TargetType *>(target)->*MemFn)());
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType(*fnptr)(const TargetType *))
        : GetterForTargetPointer([fnptr](const void *target) -> HypData
          {
              return HypData(fnptr(static_cast<const TargetType *>(target)));
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return GetterForTargetPointer.IsValid(); }

    HypData operator()(ConstAnyRef target) const
    {
        AssertThrow(IsValid());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(target.GetTypeID() == type_info.target_type_id, "Target type mismatch");
#endif

        return GetterForTargetPointer(target.GetPointer());
    }

    template <class ReturnType>
    decltype(auto) Invoke(ConstAnyRef target) const
    {
        return (*this)(target).Get<NormalizedType<ReturnType>>();
    }
};

struct HypPropertySetter
{
    Proc<void, void *, const HypData &> SetterForTargetPointer;
    HypPropertyTypeInfo                 type_info;

    HypPropertySetter() = default;

    template <class ReturnType, class TargetType, class ValueType>
    HypPropertySetter(ReturnType(TargetType::*MemFn)(ValueType))
        : SetterForTargetPointer([MemFn](void *target, const HypData &data) -> void
          {
              (static_cast<TargetType *>(target)->*MemFn)(data.Get<NormalizedType<ValueType>>());
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ValueType>();
    }

    template <class ReturnType, class TargetType, class ValueType>
    HypPropertySetter(ReturnType(*fnptr)(TargetType *, const ValueType &))
        : SetterForTargetPointer([fnptr](void *target, const HypData &data) -> void
          {
              fnptr(static_cast<TargetType *>(target), data.Get<NormalizedType<ValueType>>());
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ValueType>();
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return SetterForTargetPointer.IsValid(); }

    void operator()(AnyRef target, const HypData &value) const
    {
        AssertThrow(IsValid());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(target.GetTypeID() == type_info.target_type_id, "Target type mismatch");
#endif

        SetterForTargetPointer(target.GetPointer(), value);
    }

    template <class ValueType>
    void Invoke(AnyRef target, ValueType &&value) const
    {
        (*this)(target, HypData(std::forward<ValueType>(value)));
    }
};

struct HypProperty
{
    Name                name;
    TypeID              type_id;
    HypPropertyGetter   getter;
    HypPropertySetter   setter;

    HypProperty() = default;

    HypProperty(Name name)
        : name(name)
    {
    }

    HypProperty(Name name, HypPropertyGetter &&getter)
        : name(name),
          getter(std::move(getter))
    {
        type_id = this->getter.type_info.value_type_id;
    }

    HypProperty(Name name, HypPropertyGetter &&getter, HypPropertySetter &&setter)
        : name(name),
          getter(std::move(getter)),
          setter(std::move(setter))
    {
        type_id = this->getter.type_info.value_type_id;

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(this->setter.type_info.value_type_id == type_id, "Setter value type id should match property type id");
#endif
    }

    HypProperty(const HypProperty &other)                 = delete;
    HypProperty &operator=(const HypProperty &other)      = delete;
    HypProperty(HypProperty &&other) noexcept             = default;
    HypProperty &operator=(HypProperty &&other) noexcept  = default;

    /*! \brief Get the type id of the property, if defined. Otherwise, returns an unset type ID. */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return type_id; }

    HYP_FORCE_INLINE bool IsValid() const
        { return type_id != TypeID::Void() && HasGetter(); }

    // getter

    HYP_FORCE_INLINE bool HasGetter() const
        { return getter.IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE HypData InvokeGetter(ConstAnyRef target) const
        { return getter(target); }

    template <class ReturnType>
    HYP_NODISCARD HYP_FORCE_INLINE decltype(auto) InvokeGetter(ConstAnyRef target) const
        { return getter.Invoke<ReturnType>(target); }

    // setter

    HYP_FORCE_INLINE bool HasSetter() const
        { return setter.IsValid(); }

    HYP_FORCE_INLINE void InvokeSetter(AnyRef target, const HypData &value) const
        { setter(target, value); }

    template <class ValueType, typename = std::enable_if_t< !std::is_same_v< HypData, ValueType > > >
    HYP_FORCE_INLINE void InvokeSetter(AnyRef target, ValueType &&value) const
        { setter.Invoke<ValueType>(target, std::forward<ValueType>(value)); }

    /*! \brief Get the associated HypClass for this property's type ID, if applicable. */
    HYP_API const HypClass *GetHypClass() const;
};

} // namespace hyperion

#endif