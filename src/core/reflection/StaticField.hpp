/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/HypData.hpp>
#include <core/reflection/ClassAttribute.hpp>
#include <core/reflection/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/reflection/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Span.hpp>
#include <core/reflection/TypeInfoFwd.hpp>

#include <core/containers/String.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/serialization/fbom/FBOMData.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Class;

class StaticField : public IHypMember
{
public:
    StaticField(Name name, const TypeInfo* typeInfo, uint32 size, Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_typeInfo(typeInfo),
          m_size(size),
          m_attributes(attributes)
    {
        HYP_CORE_ASSERT(m_typeInfo != nullptr);
    }

    template <class ConstantType, typename = std::enable_if_t<!std::is_reference_v<ConstantType>>>
    StaticField(Name name, ConstantType value, Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_typeInfo(&TypeOf<NormalizedType<ConstantType>>()),
          m_size(sizeof(NormalizedType<ConstantType>)),
          m_attributes(attributes)
    {
        m_getProc = [value]() -> HypData
        {
            return HypData(value);
        };

        m_serializeProc = [value](FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
        {
            if (FBOMResult err = HypDataHelper<NormalizedType<ConstantType>>::Serialize(value, out, flags))
            {
                return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message.Data());
            }

            return {};
        };
    }

    template <class ConstantType, typename = std::enable_if_t<!std::is_reference_v<ConstantType>>>
    StaticField(Name name, const ConstantType* pValue, Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_typeInfo(&TypeOf<NormalizedType<ConstantType>>()),
          m_size(sizeof(NormalizedType<ConstantType>)),
          m_attributes(attributes)
    {
        m_getProc = [pValue]() -> HypData
        {
            return HypData(AnyRef(const_cast<NormalizedType<ConstantType>*>(pValue)));
        };

        m_serializeProc = [pValue](FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> Result
        {
            HYP_CORE_ASSERT(pValue != nullptr);

            if (FBOMResult err = HypDataHelper<NormalizedType<ConstantType>>::Serialize(*pValue, out, flags))
            {
                return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message.Data());
            }

            return {};
        };
    }

    StaticField(const StaticField& other) = delete;
    StaticField& operator=(const StaticField& other) = delete;

    StaticField(StaticField&& other) noexcept = default;
    StaticField& operator=(StaticField&& other) noexcept = default;

    virtual ~StaticField() override = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_STATIC_FIELD;
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
        return TypeInfo_Void();
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        return m_size;
    }

    virtual bool CanSerialize() const override
    {
        return IsValid() && m_serializeProc.IsValid();
    }

    virtual bool CanDeserialize() const override
    {
        return false;
    }

    HYP_FORCE_INLINE Result Serialize(FBOMData& out) const
    {
        return Serialize({}, out);
    }

    virtual Result Serialize(Span<HypData> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const override
    {
        if (!CanSerialize())
        {
            return HYP_MAKE_ERROR(Error, "Static field '{}' cannot be serialized", m_name);
        }

        if (args.Size() != 0)
        {
            return HYP_MAKE_ERROR(Error, "Expected zero arguments to serialize static field, got {}", args.Size());
        }

        return m_serializeProc(out, flags);
    }

    virtual Result Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& data) const override
    {
        return HYP_MAKE_ERROR(Error, "Static field cannot be deserialized");
    }

    virtual const ClassAttributeSet& GetAttributes() const override
    {
        return m_attributes;
    }

    virtual const ClassAttributeValue& GetAttribute(StringHash key) const override
    {
        return m_attributes.Get(key);
    }

    virtual const ClassAttributeValue& GetAttribute(StringHash key, const ClassAttributeValue& defaultValue) const override
    {
        return m_attributes.Get(key, defaultValue);
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_name.IsValid()
            && m_typeInfo && TypeInfo_GetId(*m_typeInfo) != TypeId::Void()
            && m_size != 0;
    }

    HYP_FORCE_INLINE HypData Get() const
    {
        return m_getProc();
    }

private:
    Name m_name;
    const TypeInfo* m_typeInfo;
    uint32 m_size;
    ClassAttributeSet m_attributes;

    Proc<HypData()> m_getProc;
    Proc<Result(FBOMData& out, EnumFlags<FBOMDataFlags> flags)> m_serializeProc;
};

} // namespace hyperion
