/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_PROPERTY_HPP
#define HYPERION_CORE_HYP_PROPERTY_HPP

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/containers/HashMap.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/Serialization.hpp>
#include <asset/serialization/SerializationWrapper.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

class HypMethod;
class HypField;

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
    Proc<HypData, const HypData &>          get_proc;
    Proc<fbom::FBOMData, const HypData &>   serialize_proc;
    HypPropertyTypeInfo                     type_info;

    HypPropertyGetter() = default;

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType(TargetType::*MemFn)())
        : get_proc([MemFn](const HypData &target) -> HypData
          {
              return HypData((static_cast<const TargetType *>(target.ToRef().GetPointer())->*MemFn)());
          }),
          serialize_proc([MemFn](const HypData &target) -> fbom::FBOMData
          {
              fbom::FBOMData out;

              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(HypData((static_cast<const TargetType *>(target.ToRef().GetPointer())->*MemFn)()), out)) {
                  HYP_FAIL("Failed to serialize data: %s", err.message.Data());
              }

              return out;
          })

    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType(TargetType::*MemFn)() const)
        : get_proc([MemFn](const HypData &target) -> HypData
          {
              return HypData((static_cast<const TargetType *>(target.ToRef().GetPointer())->*MemFn)());
          }),
          serialize_proc([MemFn](const HypData &target) -> fbom::FBOMData
          {
              fbom::FBOMData out;

              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(HypData((static_cast<const TargetType *>(target.ToRef().GetPointer())->*MemFn)()), out)) {
                  HYP_FAIL("Failed to serialize data: %s", err.message.Data());
              }

              return out;
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType(*fnptr)(const TargetType *))
        : get_proc([fnptr](const HypData &target) -> HypData
          {
              return HypData(fnptr(static_cast<const TargetType *>(target.ToRef().GetPointer())));
          }),
          serialize_proc([fnptr](const HypData &target) -> fbom::FBOMData
          {   
              fbom::FBOMData out;

              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(HypData(fnptr(static_cast<const TargetType *>(target.ToRef().GetPointer()))), out)) {
                  HYP_FAIL("Failed to serialize data: %s", err.message.Data());
              }

              return out;
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    // Special getter that takes no target. Used for Enums
    template <class ReturnType>
    HypPropertyGetter(ReturnType(*fnptr)(void))
        : get_proc([fnptr](const HypData &target) -> HypData
          {
              return HypData(fnptr());
          }),
          serialize_proc([fnptr](const HypData &target) -> fbom::FBOMData
          {   
              fbom::FBOMData out;

              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(HypData(fnptr()), out)) {
                  HYP_FAIL("Failed to serialize data: %s", err.message.Data());
              }

              return out;
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ReturnType>();
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t< !std::is_member_function_pointer_v<ValueType TargetType::*> > >
    explicit HypPropertyGetter(ValueType TargetType::*member)
        : get_proc([member](const HypData &target) -> HypData
          {
              return HypData(static_cast<const TargetType *>(target.ToRef().GetPointer())->*member);
          }),
          serialize_proc([member](const HypData &target) -> fbom::FBOMData
          {   
              fbom::FBOMData out;

              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Serialize(HypData(static_cast<const TargetType *>(target.ToRef().GetPointer())->*member), out)) {
                  HYP_FAIL("Failed to serialize data: %s", err.message.Data());
              }

              return out;
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ValueType>();
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return get_proc.IsValid(); }

    HypData Invoke(const HypData &target) const
    {
        AssertThrow(IsValid());
        AssertThrow(!target.IsNull());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(
            target.ToRef().Is(type_info.target_type_id),
            "Target type mismatch, expected TypeID %u, got %u",
            type_info.target_type_id.Value(),
            target.GetTypeID().Value()
        );
#endif

        return get_proc(target);
    }

    fbom::FBOMData Serialize(const HypData &target) const
    {
        AssertThrow(IsValid());
        AssertThrow(!target.IsNull());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(
            target.ToRef().Is(type_info.target_type_id),
            "Target type mismatch, expected TypeID %u, got %u",
            type_info.target_type_id.Value(),
            target.GetTypeID().Value()
        );
#endif

        return serialize_proc(target);
    }
};

struct HypPropertySetter
{
    Proc<void, HypData &, const HypData &>          set_proc;
    Proc<void, HypData &, const fbom::FBOMData &>   deserialize_proc;
    HypPropertyTypeInfo                             type_info;

    HypPropertySetter() = default;

    template <class ReturnType, class TargetType, class ValueType>
    HypPropertySetter(ReturnType(TargetType::*MemFn)(ValueType))
        : set_proc([MemFn](HypData &target, const HypData &value) -> void
          {
              if (value.IsNull()) {
                  (static_cast<TargetType *>(target.ToRef().GetPointer())->*MemFn)(NormalizedType<ValueType> { });
              } else {
                  (static_cast<TargetType *>(target.ToRef().GetPointer())->*MemFn)(value.Get<NormalizedType<ValueType>>());
              }
          }),
          deserialize_proc([MemFn](HypData &target, const fbom::FBOMData &data) -> void
          {
              HypData value;

              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(data, value)) {
                  HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
              }

              if (value.IsNull()) {
                  (static_cast<TargetType *>(target.ToRef().GetPointer())->*MemFn)(NormalizedType<ValueType> { });
              } else {
                  (static_cast<TargetType *>(target.ToRef().GetPointer())->*MemFn)(value.Get<NormalizedType<ValueType>>());
              }
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ValueType>();
    }

    template <class ReturnType, class TargetType, class ValueType>
    HypPropertySetter(ReturnType(*fnptr)(TargetType *, const ValueType &))
        : set_proc([fnptr](HypData &target, const HypData &value) -> void
          {
              if (value.IsNull()) {
                  fnptr(static_cast<TargetType *>(target.ToRef().GetPointer()), NormalizedType<ValueType> { });
              } else {
                  fnptr(static_cast<TargetType *>(target.ToRef().GetPointer()), value.Get<NormalizedType<ValueType>>());
              }
          }),
          deserialize_proc([fnptr](HypData &target, const fbom::FBOMData &data) -> void
          {
              HypData value;
    
              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(data, value)) {
                  HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
              }
    
              if (value.IsNull()) {
                  fnptr(static_cast<TargetType *>(target.ToRef().GetPointer()), NormalizedType<ValueType> { });
              } else {
                  fnptr(static_cast<TargetType *>(target.ToRef().GetPointer()), value.Get<NormalizedType<ValueType>>());
              }
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ValueType>();
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t< !std::is_member_function_pointer_v<ValueType TargetType::*> > >
    HypPropertySetter(ValueType TargetType::*member)
        : set_proc([member](HypData &target, const HypData &value) -> void
          {
              if (value.IsNull()) {
                  static_cast<TargetType *>(target.ToRef().GetPointer())->*member = NormalizedType<ValueType> { };
              } else {
                  static_cast<TargetType *>(target.ToRef().GetPointer())->*member = value.Get<NormalizedType<ValueType>>();
              }
          }),
          deserialize_proc([member](HypData &target, const fbom::FBOMData &data) -> void
          {
              HypData value;
        
              if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(data, value)) {
                  HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
              }

              if (value.IsNull()) {
                  static_cast<TargetType *>(target.ToRef().GetPointer())->*member = NormalizedType<ValueType> { };
              } else {
                  static_cast<TargetType *>(target.ToRef().GetPointer())->*member = value.Get<NormalizedType<ValueType>>();
              }
          })
    {
        type_info.value_type_id = detail::GetUnwrappedSerializationTypeID<ValueType>();
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return set_proc.IsValid(); }

    void Invoke(HypData &target, const HypData &value) const
    {
        AssertThrow(IsValid());
        AssertThrow(!target.IsNull());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(
            target.ToRef().Is(type_info.target_type_id),
            "Target type mismatch, expected TypeID %u, got %u",
            type_info.target_type_id.Value(),
            target.GetTypeID().Value()
        );
#endif

        set_proc(target, value);
    }

    void Deserialize(HypData &target, const fbom::FBOMData &value) const
    {
        AssertThrow(IsValid());
        AssertThrow(!target.IsNull());

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(
            target.ToRef().Is(type_info.target_type_id),
            "Target type mismatch, expected TypeID %u, got %u",
            type_info.target_type_id.Value(),
            target.GetTypeID().Value()
        );
#endif

        deserialize_proc(target, value);
    }
};

class HypProperty : public IHypMember
{
public:
    friend class HypClass;

    HypProperty() = default;

    HypProperty(Name name, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_attributes(attributes)
    {
    }

    HypProperty(Name name, HypPropertyGetter &&getter, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_type_id(getter.type_info.value_type_id),
          m_attributes(attributes),
          m_getter(std::move(getter))
    {
    }

    HypProperty(Name name, HypPropertyGetter &&getter, HypPropertySetter &&setter, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_type_id(getter.type_info.value_type_id),
          m_attributes(attributes),
          m_getter(std::move(getter)),
          m_setter(std::move(setter))
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(m_setter.type_info.value_type_id == m_type_id, "Setter value type id should match property type id");
#endif
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t< !std::is_member_function_pointer_v<ValueType TargetType::*> > >
    HypProperty(Name name, ValueType TargetType::*member, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_attributes(attributes),
          m_getter(HypPropertyGetter(member)),
          m_setter(HypPropertySetter(member))
    {
        m_type_id = m_getter.type_info.value_type_id;
    }

    HypProperty(const HypProperty &other)                   = delete;
    HypProperty &operator=(const HypProperty &other)        = delete;
    HypProperty(HypProperty &&other) noexcept               = default;
    HypProperty &operator=(HypProperty &&other) noexcept    = default;

    virtual ~HypProperty() override                         = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_PROPERTY;
    }

    virtual Name GetName() const override
    {
        return m_name;
    }

    virtual TypeID GetTypeID() const override
    {
        return m_type_id;
    }

    virtual TypeID GetTargetTypeID() const override
    {
        return m_getter.IsValid() ? m_getter.type_info.target_type_id
            : m_setter.IsValid() ? m_setter.type_info.target_type_id
            : TypeID::Void();
    }
    
    virtual const HypClassAttributeSet &GetAttributes() const override
    {
        return m_attributes;
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key) const override
    {
        return m_attributes.Get(key);
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key, const HypClassAttributeValue &default_value) const override
    {
        return m_attributes.Get(key, default_value);
    }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_type_id != TypeID::Void() && CanGet(); }

    // getter

    HYP_FORCE_INLINE bool CanGet() const
        { return m_getter.IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE HypData Get(const HypData &target) const
        { return m_getter.Invoke(target); }

    // Serializing getter

    HYP_FORCE_INLINE bool CanSerialize() const
        { return m_getter.IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE fbom::FBOMData Serialize(const HypData &target) const
        { return m_getter.Serialize(target); }

    // setter

    HYP_FORCE_INLINE bool CanSet() const
        { return m_setter.IsValid(); }

    HYP_FORCE_INLINE void Set(HypData &target, const HypData &value) const
        { m_setter.Invoke(target, value); }

    HYP_FORCE_INLINE bool CanDeserialize() const
        { return m_setter.IsValid(); }

    HYP_FORCE_INLINE void Deserialize(HypData &target, const fbom::FBOMData &serialized_value) const
        { m_setter.Deserialize(target, serialized_value); }

    /*! \brief Get the associated HypClass for this property's type ID, if applicable. */
    HYP_API const HypClass *GetHypClass() const;

    static HYP_API HypProperty MakeHypProperty(const HypField *field);
    static HYP_API HypProperty MakeHypProperty(const HypMethod *getter, const HypMethod *setter);

private:
    Name                    m_name;
    TypeID                  m_type_id;

    HypClassAttributeSet    m_attributes;

    HypPropertyGetter       m_getter;
    HypPropertySetter       m_setter;

};

} // namespace hyperion

#endif