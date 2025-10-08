/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/TypeInfoFwd.hpp>

#include <core/containers/String.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/serialization/fbom/FBOMData.hpp>

#include <core/Types.hpp>

namespace hyperion {

class HypClass;

class HypConstant : public IHypMember
{
public:
    HypConstant(Name name, const TypeInfo* typeInfo, uint32 size, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_typeInfo(typeInfo),
          m_size(size),
          m_attributes(attributes)
    {
        HYP_CORE_ASSERT(m_typeInfo != nullptr);
    }

    template <class ConstantType, typename = std::enable_if_t<!std::is_reference_v<ConstantType>>>
    HypConstant(Name name, ConstantType value, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_typeInfo(&TypeInfo_ForType<NormalizedType<ConstantType>>()),
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
    HypConstant(Name name, const ConstantType* pValue, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_typeInfo(&TypeInfo_ForType<NormalizedType<ConstantType>>()),
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

    HypConstant(const HypConstant& other) = delete;
    HypConstant& operator=(const HypConstant& other) = delete;

    HypConstant(HypConstant&& other) noexcept = default;
    HypConstant& operator=(HypConstant&& other) noexcept = default;

    virtual ~HypConstant() override = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_CONSTANT;
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
            return HYP_MAKE_ERROR(Error, "Constant '{}' cannot be serialized", m_name);
        }

        if (args.Size() != 0)
        {
            return HYP_MAKE_ERROR(Error, "Expected zero arguments to serialize constant, got {}", args.Size());
        }

        return m_serializeProc(out, flags);
    }

    virtual Result Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& data) const override
    {
        return HYP_MAKE_ERROR(Error, "Constant cannot be deserialized");
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
    HypClassAttributeSet m_attributes;

    Proc<HypData()> m_getProc;
    Proc<Result(FBOMData& out, EnumFlags<FBOMDataFlags> flags)> m_serializeProc;
};

} // namespace hyperion
