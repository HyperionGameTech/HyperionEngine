/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_PROPERTY_HPP
#define HYPERION_CORE_HYP_CLASS_PROPERTY_HPP

#include <core/HypClassPropertySerializer.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/functional/Proc.hpp>
#include <core/utilities/TypeID.hpp>
#include <core/memory/Any.hpp>

#include <asset/serialization/Serialization.hpp>
#include <asset/serialization/SerializationWrapper.hpp>
#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

struct HypClassPropertyTypeInfo
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

struct HypClassPropertyGetter
{
    Proc<fbom::FBOMData, const void *>              GetterForTargetPointer;
    Proc<fbom::FBOMData, const fbom::FBOMData &>    GetterForTargetData;
    HypClassPropertyTypeInfo                        type_info;

    HypClassPropertyGetter() = default;

    template <class ReturnType, class TargetType>
    HypClassPropertyGetter(ReturnType(TargetType::*MemFn)())
        : GetterForTargetPointer([MemFn](const void *target) -> fbom::FBOMData
          {
              return GetClassPropertySerializer<NormalizedType<ReturnType>>().Serialize((static_cast<TargetType *>(target)->*MemFn)());
          }),
          GetterForTargetData([MemFn](const fbom::FBOMData &target_data) -> fbom::FBOMData
          {
              Optional<const fbom::FBOMDeserializedObject &> deserialized_object = target_data.GetDeserializedObject();

#ifdef HYP_DEBUG_MODE
              AssertThrowMsg(deserialized_object.HasValue(), "Object has no in-memory representation");
#endif

              const TargetType &unwrapped = SerializationWrapper<TargetType>::Unwrap(deserialized_object->Get<TargetType>());

              return GetClassPropertySerializer<NormalizedType<ReturnType>>().Serialize((unwrapped.*MemFn)());
          })

    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypClassPropertyGetter(ReturnType(TargetType::*MemFn)() const)
        : GetterForTargetPointer([MemFn](const void *target) -> fbom::FBOMData
          {
              return GetClassPropertySerializer<NormalizedType<ReturnType>>().Serialize((static_cast<const TargetType *>(target)->*MemFn)());
          }),
          GetterForTargetData([MemFn](const fbom::FBOMData &target_data) -> fbom::FBOMData
          {
              Optional<const fbom::FBOMDeserializedObject &> deserialized_object = target_data.GetDeserializedObject();

#ifdef HYP_DEBUG_MODE
              AssertThrowMsg(deserialized_object.HasValue(), "Object has no in-memory representation");
#endif

              const TargetType &unwrapped = SerializationWrapper<TargetType>::Unwrap(deserialized_object->Get<TargetType>());

              return GetClassPropertySerializer<NormalizedType<ReturnType>>().Serialize((unwrapped.*MemFn)());
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    explicit operator bool() const
        { return IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsValid() const
        { return GetterForTargetPointer.IsValid() && GetterForTargetData.IsValid(); }

    fbom::FBOMData operator()(const fbom::FBOMData &target) const
    {
        AssertThrow(IsValid());

        return GetterForTargetData(target);
    }

    template <class ReturnType>
    decltype(auto) Invoke(const fbom::FBOMData &target) const
    {
        return GetClassPropertySerializer<NormalizedType<ReturnType>>().Deserialize((*this)(target));
    }

    template <class TargetType, typename = std::enable_if_t< !std::is_same_v<TargetType, fbom::FBOMData> > >
    fbom::FBOMData operator()(const TargetType &target) const
    {
        AssertThrow(IsValid());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(TypeID::ForType<NormalizedType<TargetType>>() == type_info.target_type_id, "Target type mismatch");
#endif

        return GetterForTargetPointer(&target);
    }

    template <class ReturnType, class TargetType, typename = std::enable_if_t< !std::is_same_v<TargetType, fbom::FBOMData> > >
    decltype(auto) Invoke(const TargetType &target) const
    {
        return GetClassPropertySerializer<NormalizedType<ReturnType>>().Deserialize((*this)(target));
    }

    template <class ReturnType, class TargetType, typename = std::enable_if_t< !std::is_same_v<TargetType, fbom::FBOMData> > >
    decltype(auto) Invoke(const TargetType *target) const
    {
        AssertThrow(target != nullptr);

        return Invoke<ReturnType, TargetType>(*target);
    }
};

struct HypClassPropertySetter
{
    Proc<void, void *, const fbom::FBOMData &>              SetterForTargetPointer;
    Proc<void, fbom::FBOMData &, const fbom::FBOMData &>    SetterForTargetData;

    HypClassPropertyTypeInfo                                type_info;

    HypClassPropertySetter() = default;

    template <class ReturnType, class TargetType, class ValueType>
    HypClassPropertySetter(ReturnType(TargetType::*MemFn)(ValueType))
        : SetterForTargetPointer([MemFn](void *target, const fbom::FBOMData &data) -> void
          {
              (static_cast<TargetType *>(target)->*MemFn)(GetClassPropertySerializer<NormalizedType<ValueType>>().Deserialize(data));
          }),
          SetterForTargetData([MemFn](fbom::FBOMData &target_data, const fbom::FBOMData &data) -> void
          {
              Optional<fbom::FBOMDeserializedObject &> deserialized_object = target_data.GetDeserializedObject();

#ifdef HYP_DEBUG_MODE
              AssertThrowMsg(deserialized_object.HasValue(), "Object has no in-memory representation");
#endif

              TargetType &unwrapped = SerializationWrapper<TargetType>::Unwrap(deserialized_object->Get<typename SerializationWrapper<TargetType>::Type>());

              (unwrapped.*MemFn)(GetClassPropertySerializer<NormalizedType<ValueType>>().Deserialize(data));
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ValueType>();
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    explicit operator bool() const
        { return IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsValid() const
        { return SetterForTargetPointer.IsValid() && SetterForTargetData.IsValid(); }

    void operator()(fbom::FBOMData &target, const fbom::FBOMData &value) const
    {
        AssertThrow(IsValid());

        SetterForTargetData(target, value);
    }

    template <class ValueType>
    void Invoke(fbom::FBOMData &target, ValueType &&value) const
    {
        (*this)(target, GetClassPropertySerializer<NormalizedType<ValueType>>().Serialize(std::forward<ValueType>(value)));
    }

    template <class TargetType, typename = std::enable_if_t< !std::is_same_v<TargetType, fbom::FBOMData> > >
    void operator()(TargetType &target, const fbom::FBOMData &value) const
    {
        AssertThrow(IsValid());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(TypeID::ForType<NormalizedType<TargetType>>() == type_info.target_type_id, "Target type mismatch");
#endif

        SetterForTargetPointer(&target, value);
    }

    template <class TargetType, class ValueType, typename = std::enable_if_t< !std::is_same_v<TargetType, fbom::FBOMData> > >
    void Invoke(TargetType &target, ValueType &&value) const
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(TypeID::ForType<NormalizedType<ValueType>>() == type_info.value_type_id, "Value type mismatch");
#endif

        (*this)(target, GetClassPropertySerializer<NormalizedType<ValueType>>().Serialize(std::forward<ValueType>(value)));
    }

    template <class TargetType, class ValueType, typename = std::enable_if_t< !std::is_same_v<TargetType, fbom::FBOMData> > >
    void Invoke(TargetType *target, ValueType &&value) const
    {
        AssertThrow(target != nullptr);

        return Invoke<TargetType, ValueType>(*target, std::forward<ValueType>(value));
    }
};

struct HypClassProperty
{
    Name                    name;
    TypeID                  type_id;
    HypClassPropertyGetter  getter;
    HypClassPropertySetter  setter;

    HypClassProperty() = default;

    HypClassProperty(Name name)
        : name(name)
    {
    }

    HypClassProperty(Name name, HypClassPropertyGetter &&getter)
        : name(name),
          getter(std::move(getter))
    {
        type_id = this->getter.type_info.value_type_id;
    }

    HypClassProperty(Name name, HypClassPropertyGetter &&getter, HypClassPropertySetter &&setter)
        : name(name),
          getter(std::move(getter)),
          setter(std::move(setter))
    {
        type_id = this->getter.type_info.value_type_id;

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(this->setter.type_info.value_type_id == type_id, "Setter value type id should match property type id");
#endif
    }

    HypClassProperty(const HypClassProperty &other)                 = delete;
    HypClassProperty &operator=(const HypClassProperty &other)      = delete;
    HypClassProperty(HypClassProperty &&other) noexcept             = default;
    HypClassProperty &operator=(HypClassProperty &&other) noexcept  = default;

    /*! \brief Get the type id of the property, if defined. Otherwise, returns an unset type ID. */
    HYP_NODISCARD HYP_FORCE_INLINE
    TypeID GetTypeID() const
        { return type_id; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsValid() const
        { return type_id != TypeID::Void() && HasGetter(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool HasGetter() const
        { return getter.IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    fbom::FBOMData InvokeGetter(const fbom::FBOMData &target) const
        { return getter(target); }

    template <class ReturnType>
    HYP_NODISCARD HYP_FORCE_INLINE
    decltype(auto) InvokeGetter(const fbom::FBOMData &target) const
        { return getter.Invoke<ReturnType>(target); }

    template <class TargetType, typename = std::enable_if_t< !std::is_same_v< fbom::FBOMData, TargetType > > >
    HYP_NODISCARD HYP_FORCE_INLINE
    fbom::FBOMData InvokeGetter(const TargetType &target) const
        { return getter(target); }

    template <class ReturnType, class TargetType, typename = std::enable_if_t< !std::is_same_v< fbom::FBOMData, TargetType > > >
    HYP_NODISCARD HYP_FORCE_INLINE
    decltype(auto) InvokeGetter(const TargetType &target) const
        { return getter.Invoke<ReturnType, TargetType>(&target); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool HasSetter() const
        { return setter.IsValid(); }

    HYP_FORCE_INLINE
    void InvokeSetter(fbom::FBOMData &target, const fbom::FBOMData &value) const
        { setter(target, value); }

    HYP_FORCE_INLINE
    void InvokeSetter(fbom::FBOMData &target, fbom::FBOMData &&value) const
        { setter(target, std::move(value)); }

    template <class ValueType, typename = std::enable_if_t< !std::is_same_v< fbom::FBOMData, ValueType > > >
    HYP_FORCE_INLINE
    void InvokeSetter(fbom::FBOMData &target, ValueType &&value) const
        { setter.Invoke<ValueType>(target, std::forward<ValueType>(value)); }

    HYP_FORCE_INLINE
    void InvokeSetter(fbom::FBOMData &&target, const fbom::FBOMData &value) const
        { setter(target, value); }

    HYP_FORCE_INLINE
    void InvokeSetter(fbom::FBOMData &&target, fbom::FBOMData &&value) const
        { setter(target, std::move(value)); }

    template <class ValueType, typename = std::enable_if_t< !std::is_same_v< fbom::FBOMData, ValueType > > >
    HYP_FORCE_INLINE
    void InvokeSetter(fbom::FBOMData &&target, ValueType &&value) const
        { setter.Invoke<ValueType>(target, std::forward<ValueType>(value)); }

    template <class TargetType, typename = std::enable_if_t< !std::is_same_v< fbom::FBOMData, TargetType > > >
    HYP_FORCE_INLINE
    void InvokeSetter(TargetType &target, const fbom::FBOMData &value) const
        { setter(target, value); }

    template <class TargetType, class ValueType, typename = std::enable_if_t< !std::is_same_v< fbom::FBOMData, TargetType > && !std::is_same_v< fbom::FBOMData, ValueType > > >
    HYP_FORCE_INLINE
    void InvokeSetter(TargetType &target, ValueType &&value) const
        { setter.Invoke<TargetType, ValueType>(&target, std::forward<ValueType>(value)); }


    /*! \brief Get the associated HypClass for this property's type ID, if applicable. */
    HYP_API const HypClass *GetHypClass() const;
};

} // namespace hyperion

#endif