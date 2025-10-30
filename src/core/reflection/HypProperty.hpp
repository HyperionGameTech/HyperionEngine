/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/HypData.hpp>
#include <core/reflection/HypClassAttribute.hpp>
#include <core/reflection/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/containers/HashMap.hpp>

#include <core/reflection/TypeId.hpp>
#include <core/reflection/TypeInfoFwd.hpp>
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
    const TypeInfo* targetTypeInfo = &TypeInfo_Void();
    const TypeInfo* valueTypeInfo = &TypeInfo_Void(); // for getter or setter: getter is param type, setter is return type
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
    Proc<HypData(const HypData& target)> getProc;
    Proc<Result(const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags)> serializeProc;
    HypPropertyTypeInfo typeInfo;

    HypPropertyGetter() = default;

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType (TargetType::*memFn)())
        : getProc([memFn](const HypData& target) -> HypData
              {
                  return HypData((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)());
              }),
          serializeProc([memFn](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
              {
                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)(), out, flags))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message);
                  }

                  return {};
              })

    {
        typeInfo.valueTypeInfo = &TypeOf<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType (TargetType::*memFn)() const)
        : getProc([memFn](const HypData& target) -> HypData
              {
                  return HypData((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)());
              }),
          serializeProc([memFn](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
              {
                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize((static_cast<const TargetType*>(target.ToRef().GetPointer())->*memFn)(), out, flags))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message);
                  }

                  return {};
              })
    {
        typeInfo.valueTypeInfo = &TypeOf<ReturnType>();
    }

    template <class ReturnType, class TargetType>
    HypPropertyGetter(ReturnType (*fnptr)(const TargetType*))
        : getProc([fnptr](const HypData& target) -> HypData
              {
                  return HypData(fnptr(static_cast<const TargetType*>(target.ToRef().GetPointer())));
              }),
          serializeProc([fnptr](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
              {
                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(fnptr(static_cast<const TargetType*>(target.ToRef().GetPointer())), out, flags))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message);
                  }

                  return {};
              })
    {
        typeInfo.valueTypeInfo = &TypeOf<ReturnType>();
    }

    // Special getter that takes no target. Used for Enums
    template <class ReturnType>
    HypPropertyGetter(ReturnType (*fnptr)(void))
        : getProc([fnptr](const HypData& target) -> HypData
              {
                  return HypData(fnptr());
              }),
          serializeProc([fnptr](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
              {
                  if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(fnptr(), out, flags))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message);
                  }

                  return {};
              })
    {
        typeInfo.valueTypeInfo = &TypeOf<ReturnType>();
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t<!std::is_member_function_pointer_v<ValueType TargetType::*>>>
    explicit HypPropertyGetter(ValueType TargetType::* member)
        : getProc([member](const HypData& target) -> HypData
              {
                  return HypData(static_cast<const TargetType*>(target.ToRef().GetPointer())->*member);
              }),
          serializeProc([member](const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
              {
                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Serialize(static_cast<const TargetType*>(target.ToRef().GetPointer())->*member, out, flags))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message);
                  }

                  return {};
              })
    {
        typeInfo.valueTypeInfo = &TypeOf<ValueType>();
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
            target.ToRef().Is(TypeInfo_GetId(*typeInfo.targetTypeInfo)),
            "Target type mismatch, expected %s, got %s",
            *TypeInfo_GetName(*typeInfo.targetTypeInfo),
            *TypeInfo_GetName(*target.GetTypeInfo()));

        return getProc(target);
    }

    Result Serialize(const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags) const
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(!target.IsNull());

        HYP_CORE_ASSERT(
            target.ToRef().Is(TypeInfo_GetId(*typeInfo.targetTypeInfo)),
            "Target type mismatch, expected %s, got %s",
            *TypeInfo_GetName(*typeInfo.targetTypeInfo),
            *TypeInfo_GetName(*target.GetTypeInfo()));

        return serializeProc(target, out, flags);
    }
};

struct HypPropertySetter
{
    Proc<void(HypData&, const HypData&)> setProc;
    Proc<Result(FBOMLoadContext&, HypData&, const FBOMData&)> deserializeProc;
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
          deserializeProc([memFn](FBOMLoadContext& context, HypData& target, const FBOMData& data) -> Result
              {
                  HypData value;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(context, data, value))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to deserialize data: {}", err.message);
                  }

                  if (value.IsNull())
                  {
                      (static_cast<TargetType*>(target.ToRef().GetPointer())->*memFn)(NormalizedType<ValueType> {});
                  }
                  else
                  {
                      (static_cast<TargetType*>(target.ToRef().GetPointer())->*memFn)(value.Get<NormalizedType<ValueType>>());
                  }

                  return {};
              })
    {
        typeInfo.valueTypeInfo = &TypeOf<ValueType>();
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
          deserializeProc([fnptr](FBOMLoadContext& context, HypData& target, const FBOMData& data) -> Result
              {
                  HypData value;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(context, data, value))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to deserialize data: {}", err.message);
                  }

                  if (value.IsNull())
                  {
                      fnptr(static_cast<TargetType*>(target.ToRef().GetPointer()), NormalizedType<ValueType> {});
                  }
                  else
                  {
                      fnptr(static_cast<TargetType*>(target.ToRef().GetPointer()), value.Get<NormalizedType<ValueType>>());
                  }

                  return {};
              })
    {
        typeInfo.valueTypeInfo = &TypeOf<ValueType>();
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
          deserializeProc([member](FBOMLoadContext& context, HypData& target, const FBOMData& data) -> Result
              {
                  HypData value;

                  if (FBOMResult err = HypDataHelper<NormalizedType<ValueType>>::Deserialize(context, data, value))
                  {
                      return HYP_MAKE_ERROR(Error, "Failed to deserialize data: {}", err.message);
                  }

                  if (value.IsNull())
                  {
                      static_cast<TargetType*>(target.ToRef().GetPointer())->*member = NormalizedType<ValueType> {};
                  }
                  else
                  {
                      static_cast<TargetType*>(target.ToRef().GetPointer())->*member = value.Get<NormalizedType<ValueType>>();
                  }

                  return {};
              })
    {
        typeInfo.valueTypeInfo = &TypeOf<ValueType>();
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
            target.ToRef().Is(TypeInfo_GetId(*typeInfo.targetTypeInfo)),
            "Target type mismatch, expected %s, got %s",
            *TypeInfo_GetName(*typeInfo.targetTypeInfo),
            *TypeInfo_GetName(*target.GetTypeInfo()));

        setProc(target, value);
    }

    Result Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& value) const
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(!target.IsNull());

        HYP_CORE_ASSERT(
            target.ToRef().Is(TypeInfo_GetId(*typeInfo.targetTypeInfo)),
            "Target type mismatch, expected %s, got %s",
            *TypeInfo_GetName(*typeInfo.targetTypeInfo),
            *TypeInfo_GetName(*target.GetTypeInfo()));

        return deserializeProc(context, target, value);
    }
};

class HypProperty final : public IHypMember
{
public:
    friend class HypClass;
    friend HypProperty* MakeHypProperty(const HypField* field, const HypMethod* getter, const HypMethod* setter);

    HypProperty()
        : m_typeInfo(&TypeInfo_Void()),
          m_originalMember(nullptr)
    {
    }

    HypProperty(Name name, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeInfo(&TypeInfo_Void()),
          m_attributes(attributes),
          m_originalMember(nullptr)
    {
    }

    HypProperty(Name name, const TypeInfo* typeInfo, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeInfo(typeInfo),
          m_attributes(attributes),
          m_originalMember(nullptr)
    {
        HYP_CORE_ASSERT(m_typeInfo != nullptr);
    }

    HypProperty(Name name, HypPropertyGetter&& getter, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeInfo(getter.typeInfo.valueTypeInfo),
          m_attributes(attributes),
          m_getter(std::move(getter)),
          m_originalMember(nullptr)
    {
        HYP_CORE_ASSERT(m_typeInfo != nullptr);
    }

    HypProperty(Name name, HypPropertyGetter&& getter, HypPropertySetter&& setter, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeInfo(getter.typeInfo.valueTypeInfo),
          m_attributes(attributes),
          m_getter(std::move(getter)),
          m_setter(std::move(setter)),
          m_originalMember(nullptr)
    {
        HYP_CORE_ASSERT(m_typeInfo != nullptr);
        HYP_CORE_ASSERT(TypeInfo_GetId(*m_setter.typeInfo.valueTypeInfo) == TypeInfo_GetId(*m_typeInfo), "Setter value type id should match property type id");
    }

    template <class ValueType, class TargetType, typename = std::enable_if_t<!std::is_member_function_pointer_v<ValueType TargetType::*>>>
    HypProperty(Name name, ValueType TargetType::* member, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_attributes(attributes),
          m_getter(HypPropertyGetter(member)),
          m_setter(HypPropertySetter(member)),
          m_originalMember(nullptr)
    {
        m_typeInfo = m_getter.typeInfo.valueTypeInfo;
        HYP_CORE_ASSERT(m_typeInfo != nullptr);
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

    virtual const TypeInfo& GetTypeInfo() const override
    {
        return *m_typeInfo;
    }

    virtual const TypeInfo& GetTargetTypeInfo() const override
    {
        return m_getter.IsValid()
            ? *m_getter.typeInfo.targetTypeInfo
            : (m_setter.IsValid()
                      ? *m_setter.typeInfo.targetTypeInfo
                      : TypeInfo_Void());
    }

    virtual bool CanSerialize() const override
    {
        return m_getter.IsValid();
    }

    virtual bool CanDeserialize() const override
    {
        return m_setter.IsValid();
    }

    virtual Result Serialize(Span<HypData> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const override
    {
        if (!CanSerialize())
        {
            return HYP_MAKE_ERROR(Error, "Property cannot be serialized");
        }

        if (args.Size() != 1)
        {
            return HYP_MAKE_ERROR(Error, "Expected exactly one argument to serialize property, got {}", args.Size());
        }

        return m_getter.Serialize(*args.Data(), out, flags);
    }

    virtual Result Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& serializedValue) const override
    {
        if (!CanDeserialize())
        {
            return HYP_MAKE_ERROR(Error, "Property cannot be deserialized");
        }

        return m_setter.Deserialize(context, target, serializedValue);
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
        return m_typeInfo && TypeInfo_GetId(*m_typeInfo) != TypeId::Void() && CanGet();
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
    const TypeInfo* m_typeInfo;

    HypClassAttributeSet m_attributes;

    HypPropertyGetter m_getter;
    HypPropertySetter m_setter;

    // Set when this property is synthesized from a field or method
    const IHypMember* m_originalMember;
};

} // namespace hyperion
