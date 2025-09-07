/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/containers/HashMap.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Span.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/serialization/Serialization.hpp>
#include <core/serialization/SerializationWrapper.hpp>

#include <core/Types.hpp>

namespace hyperion {

class HypClass;

class HypMethod;
class HypField;

struct HypPropertyTypeInfo
{
    TypeId targetTypeId;
    TypeId valueTypeId; // for getter or setter: getter is param type, setter is return type
};

template <class T>
using UnwrappedSerializationType = NormalizedType<typename SerializationWrapperReverseMapping<NormalizedType<T>>::Type>;

template <class T>
constexpr TypeId GetUnwrappedSerializationTypeId()
{
    return TypeId::ForType<UnwrappedSerializationType<T>>();
}

struct HypPropertyGetter
{
    Proc<HypData(const HypData&)> getProc;
    Proc<FBOMData(const HypData&, EnumFlags<FBOMDataFlags> flags)> serializeProc;
    HypPropertyTypeInfo typeInfo;

    HypPropertyGetter() = default;

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType (TargetType::*memFn)())
        : getProc([memFn](const HypData& target) -> HypData
              {
                  return HypData((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)());
              }),
          serializeProc([memFn](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
              {
                  FBOMData out;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)(), out, flags))
                  {
                      HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                  }

                  return out;
              })

    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType (TargetType::*memFn)() const)
        : getProc([memFn](const HypData& target) -> HypData
              {
                  return HypData((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)());
              }),
          serializeProc([memFn](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
              {
                  FBOMData out;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)(), out, flags))
                  {
                      HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                  }

                  return out;
              })
    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType (*fnptr)(const TargetType*))
        : getProc([fnptr](const HypData& target) -> HypData
              {
                  return HypData(fnptr(static_cast<const TargetType*>(target.ToRef().GetPointer())));
              }),
          serializeProc([fnptr](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
              {
                  FBOMData out;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(fnptr(static_cast<const TargetType*>(target.ToRef().GetPointer())), out, flags))
                  {
                      HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                  }

                  return out;
              })
    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ReturnType>();
    }

    // Special getter that takes no target. Used for Enums
    template <class ReturnType>
    HypPropertyGetter(ReturnType (*fnptr)(void))
        : getProc([fnptr](const HypData& target) -> HypData
              {
                  return HypData(fnptr());
              }),
          serializeProc([fnptr](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
              {
                  FBOMData out;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(fnptr(), out, flags))
                  {
                      HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                  }

                  return out;
              })
    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ReturnType>();
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t<!std::is_member_function_pointer_v<ValueType TargetType::*>>>
    explicit HypPropertyGetter(ValueType TargetType::* member)
        : getProc([member](const HypData& target) -> HypData
              {
                  return HypData(static_cast<const TargetType*>(target.ToRef().GetPointer())->*member);
              }),
          serializeProc([member](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
              {
                  FBOMData out;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Serialize(static_cast<const TargetType*>(target.ToRef().GetPointer())->*member, out, flags))
                  {
                      HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                  }

                  return out;
              })
    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ValueType>();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return getProc.IsValid();
    }

    HypData Invoke(const HypData& target) const
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(!target.IsNull());

        HYP_CORE_ASSERT(
            target.ToRef().Is(typeInfo.targetTypeId),
            "Target type mismatch, expected TypeId %u, got %u",
            typeInfo.targetTypeId.Value(),
            target.GetTypeId().Value());

        return getProc(target);
    }

    FBOMData Serialize(const HypData& target, EnumFlags<FBOMDataFlags> flags) const
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(!target.IsNull());

        HYP_CORE_ASSERT(
            target.ToRef().Is(typeInfo.targetTypeId),
            "Target type mismatch, expected TypeId %u, got %u",
            typeInfo.targetTypeId.Value(),
            target.GetTypeId().Value());

        return serializeProc(target, flags);
    }
};

struct HypPropertySetter
{
    Proc<void(HypData&, const HypData&)> setProc;
    Proc<void(FBOMLoadContext&, HypData&, const FBOMData&)> deserializeProc;
    HypPropertyTypeInfo typeInfo;

    HypPropertySetter() = default;

    template <class ReturnType, class TargetType, class ValueType>
    HypPropertySetter(ReturnType (TargetType::*memFn)(ValueType))
        : setProc([memFn](HypData& target, const HypData& value) -> void
              {
                  if (value.IsNull())
                  {
                      (static_cast<TargetType*>(target.ToRef().GetPointer())->*memFn)(NormalizedType<ValueType> {});
                  }
                  else
                  {
                      (static_cast<TargetType*>(target.ToRef().GetPointer())->*memFn)(value.Get<NormalizedType<ValueType>>());
                  }
              }),
          deserializeProc([memFn](FBOMLoadContext& context, HypData& target, const FBOMData& data) -> void
              {
                  HypData value;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(context, data, value))
                  {
                      HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                  }

                  if (value.IsNull())
                  {
                      (static_cast<TargetType*>(target.ToRef().GetPointer())->*memFn)(NormalizedType<ValueType> {});
                  }
                  else
                  {
                      (static_cast<TargetType*>(target.ToRef().GetPointer())->*memFn)(value.Get<NormalizedType<ValueType>>());
                  }
              })
    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ValueType>();
    }

    template <class ReturnType, class TargetType, class ValueType>
    HypPropertySetter(ReturnType (*fnptr)(TargetType*, const ValueType&))
        : setProc([fnptr](HypData& target, const HypData& value) -> void
              {
                  if (value.IsNull())
                  {
                      fnptr(static_cast<TargetType*>(target.ToRef().GetPointer()), NormalizedType<ValueType> {});
                  }
                  else
                  {
                      fnptr(static_cast<TargetType*>(target.ToRef().GetPointer()), value.Get<NormalizedType<ValueType>>());
                  }
              }),
          deserializeProc([fnptr](FBOMLoadContext& context, HypData& target, const FBOMData& data) -> void
              {
                  HypData value;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(context, data, value))
                  {
                      HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                  }

                  if (value.IsNull())
                  {
                      fnptr(static_cast<TargetType*>(target.ToRef().GetPointer()), NormalizedType<ValueType> {});
                  }
                  else
                  {
                      fnptr(static_cast<TargetType*>(target.ToRef().GetPointer()), value.Get<NormalizedType<ValueType>>());
                  }
              })
    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ValueType>();
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t<!std::is_member_function_pointer_v<ValueType TargetType::*>>>
    HypPropertySetter(ValueType TargetType::* member)
        : setProc([member](HypData& target, const HypData& value) -> void
              {
                  if (value.IsNull())
                  {
                      static_cast<TargetType*>(target.ToRef().GetPointer())->*member = NormalizedType<ValueType> {};
                  }
                  else
                  {
                      static_cast<TargetType*>(target.ToRef().GetPointer())->*member = value.Get<NormalizedType<ValueType>>();
                  }
              }),
          deserializeProc([member](FBOMLoadContext& context, HypData& target, const FBOMData& data) -> void
              {
                  HypData value;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(context, data, value))
                  {
                      HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                  }

                  if (value.IsNull())
                  {
                      static_cast<TargetType*>(target.ToRef().GetPointer())->*member = NormalizedType<ValueType> {};
                  }
                  else
                  {
                      static_cast<TargetType*>(target.ToRef().GetPointer())->*member = value.Get<NormalizedType<ValueType>>();
                  }
              })
    {
        typeInfo.valueTypeId = GetUnwrappedSerializationTypeId<ValueType>();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return setProc.IsValid();
    }

    void Invoke(HypData& target, const HypData& value) const
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(!target.IsNull());

        HYP_CORE_ASSERT(
            target.ToRef().Is(typeInfo.targetTypeId),
            "Target type mismatch, expected TypeId %u, got %u",
            typeInfo.targetTypeId.Value(),
            target.GetTypeId().Value());

        setProc(target, value);
    }

    void Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& value) const
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(!target.IsNull());

        HYP_CORE_ASSERT(
            target.ToRef().Is(typeInfo.targetTypeId),
            "Target type mismatch, expected TypeId %u, got %u",
            typeInfo.targetTypeId.Value(),
            target.GetTypeId().Value());

        deserializeProc(context, target, value);
    }
};

class HypProperty : public IHypMember
{
public:
    friend class HypClass;
    friend HypProperty* MakeHypProperty(const HypField* field);
    friend HypProperty* MakeHypProperty(const HypMethod* getter, const HypMethod* setter);

    HypProperty()
        : m_originalMember(nullptr)
    {
    }

    HypProperty(Name name, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_attributes(attributes),
          m_originalMember(nullptr)
    {
    }

    HypProperty(Name name, TypeId typeId, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeId(typeId),
          m_attributes(attributes),
          m_originalMember(nullptr)
    {
    }

    HypProperty(Name name, HypPropertyGetter&& getter, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeId(getter.typeInfo.valueTypeId),
          m_attributes(attributes),
          m_getter(std::move(getter)),
          m_originalMember(nullptr)
    {
    }

    HypProperty(Name name, HypPropertyGetter&& getter, HypPropertySetter&& setter, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeId(getter.typeInfo.valueTypeId),
          m_attributes(attributes),
          m_getter(std::move(getter)),
          m_setter(std::move(setter)),
          m_originalMember(nullptr)
    {
        HYP_CORE_ASSERT(m_setter.typeInfo.valueTypeId == m_typeId, "Setter value type id should match property type id");
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t<!std::is_member_function_pointer_v<ValueType TargetType::*>>>
    HypProperty(Name name, ValueType TargetType::* member, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_attributes(attributes),
          m_getter(HypPropertyGetter(member)),
          m_setter(HypPropertySetter(member)),
          m_originalMember(nullptr)
    {
        m_typeId = m_getter.typeInfo.valueTypeId;
    }

    HypProperty(const HypProperty& other) = delete;
    HypProperty& operator=(const HypProperty& other) = delete;
    HypProperty(HypProperty&& other) noexcept = default;
    HypProperty& operator=(HypProperty&& other) noexcept = default;

    virtual ~HypProperty() override = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_PROPERTY;
    }

    virtual Name GetName() const override
    {
        return m_name;
    }

    virtual TypeId GetTypeId() const override
    {
        return m_typeId;
    }

    virtual TypeId GetTargetTypeId() const override
    {
        return m_getter.IsValid() ? m_getter.typeInfo.targetTypeId
            : m_setter.IsValid()  ? m_setter.typeInfo.targetTypeId
                                  : TypeId::Void();
    }

    virtual bool CanSerialize() const override
    {
        return m_getter.IsValid();
    }

    virtual bool CanDeserialize() const override
    {
        return m_setter.IsValid();
    }

    HYP_FORCE_INLINE bool Serialize(const HypData& target, FBOMData& out) const
    {
        return Serialize(Span<HypData>(&const_cast<HypData&>(target), 1), out);
    }

    virtual bool Serialize(Span<HypData> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const override
    {
        if (!CanSerialize())
        {
            return false;
        }

        if (args.Size() != 1)
        {
            return false;
        }

        out = m_getter.Serialize(*args.Data(), flags);

        return true;
    }

    virtual bool Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& serializedValue) const override
    {
        if (!CanDeserialize())
        {
            return false;
        }

        m_setter.Deserialize(context, target, serializedValue);

        return true;
    }

    virtual const HypClassAttributeSet& GetAttributes() const override
    {
        return m_attributes;
    }

    virtual const HypClassAttributeValue& GetAttribute(WeakName key) const override
    {
        return m_attributes.Get(key);
    }

    virtual const HypClassAttributeValue& GetAttribute(WeakName key, const HypClassAttributeValue& defaultValue) const override
    {
        return m_attributes.Get(key, defaultValue);
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_typeId != TypeId::Void() && CanGet();
    }

    // getter

    HYP_FORCE_INLINE bool CanGet() const
    {
        return m_getter.IsValid();
    }

    HYP_NODISCARD HYP_FORCE_INLINE HypData Get(const HypData& target) const
    {
        return m_getter.Invoke(target);
    }

    // setter

    HYP_FORCE_INLINE bool CanSet() const
    {
        return m_setter.IsValid();
    }

    HYP_FORCE_INLINE void Set(HypData& target, const HypData& value) const
    {
        m_setter.Invoke(target, value);
    }

    /*! \brief Get the original member that this property was synthesized from, if applicable. */
    HYP_FORCE_INLINE const IHypMember* GetOriginalMember() const
    {
        return m_originalMember;
    }

    /*! \brief Get the associated HypClass for this property's type Id, if applicable. */
    HYP_API const HypClass* GetHypClass() const;

private:
    Name m_name;
    TypeId m_typeId;

    HypClassAttributeSet m_attributes;

    HypPropertyGetter m_getter;
    HypPropertySetter m_setter;

    // Set when this property is synthesized from a field or method
    const IHypMember* m_originalMember;
};

} // namespace hyperion
