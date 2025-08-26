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
    HypConstant(Name name, TypeId typeId, uint32 size, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_typeId(typeId),
          m_size(size),
          m_attributes(attributes)
    {
    }

    template <class ConstantType, typename = std::enable_if_t<!std::is_reference_v<ConstantType>>>
    HypConstant(Name name, ConstantType value, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_typeId(TypeId::ForType<NormalizedType<ConstantType>>()),
          m_size(sizeof(NormalizedType<ConstantType>)),
          m_attributes(attributes)
    {
        m_getProc = [value]() -> HypData
        {
            return HypData(value);
        };

        if (m_attributes["serialize"] || m_attributes["xmlattribute"])
        {
            m_serializeProc = [value](EnumFlags<FBOMDataFlags> flags) -> FBOMData
            {
                FBOMData out;

                if (FBOMResult err = HypDataHelper<NormalizedType<ConstantType>>::Serialize(value, out, flags))
                {
                    HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                }

                return out;
            };
        }
    }

    template <class ConstantType, typename = std::enable_if_t<!std::is_reference_v<ConstantType>>>
    HypConstant(Name name, const ConstantType* valuePtr, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_typeId(TypeId::ForType<NormalizedType<ConstantType>>()),
          m_size(sizeof(NormalizedType<ConstantType>)),
          m_attributes(attributes)
    {
        m_getProc = [valuePtr]() -> HypData
        {
            return HypData(AnyRef(const_cast<NormalizedType<ConstantType>*>(valuePtr)));
        };

        if (m_attributes["serialize"] || m_attributes["xmlattribute"])
        {
            m_serializeProc = [valuePtr](EnumFlags<FBOMDataFlags> flags) -> FBOMData
            {
                HYP_CORE_ASSERT(valuePtr != nullptr);

                FBOMData out;

                if (FBOMResult err = HypDataHelper<NormalizedType<ConstantType>>::Serialize(*valuePtr, out, flags))
                {
                    HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                }

                return out;
            };
        }
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

    virtual TypeId GetTypeId() const override
    {
        return m_typeId;
    }

    virtual TypeId GetTargetTypeId() const override
    {
        return TypeId::Void();
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

    HYP_FORCE_INLINE bool Serialize(FBOMData& out) const
    {
        return Serialize({}, out);
    }

    virtual bool Serialize(Span<HypData> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const override
    {
        if (!CanSerialize())
        {
            return false;
        }

        if (args.Size() != 0)
        {
            return false;
        }

        out = m_serializeProc(flags);

        return true;
    }

    virtual bool Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& data) const override
    {
        return false;
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
            && m_typeId != TypeId::Void()
            && m_size != 0;
    }

    HYP_FORCE_INLINE HypData Get() const
    {
        return m_getProc();
    }

private:
    Name m_name;
    TypeId m_typeId;
    uint32 m_size;
    HypClassAttributeSet m_attributes;

    Proc<HypData()> m_getProc;
    Proc<FBOMData(EnumFlags<FBOMDataFlags> flags)> m_serializeProc;
};

} // namespace hyperion
