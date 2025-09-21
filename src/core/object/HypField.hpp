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
#include <core/utilities/Result.hpp>

#include <core/containers/String.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/serialization/fbom/FBOMData.hpp>

#include <core/Types.hpp>

namespace hyperion {

class HypClass;

class HypField final : public IHypMember
{
public:
    HypField(const Span<const HypClassAttribute>& attributes = {})
        : m_name(Name::Invalid()),
          m_typeId(TypeId::Void()),
          m_targetTypeId(TypeId::Void()),
          m_offset(~0u),
          m_size(0),
          m_attributes(attributes)
    {
    }

    /*! \brief Script object (HypObjectBase) overload */
    HypField(Name name, TypeId typeId, TypeId targetTypeId, uint32 offset, uint32 size, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeId(typeId),
          m_targetTypeId(targetTypeId),
          m_offset(offset),
          m_size(size),
          m_attributes(attributes)
    {
        HYP_CORE_ASSERT(typeId == TypeId::ForType<HypData>(), "HypField typeId must be HypData");

        m_getProc = [offset](const HypData& targetData) -> HypData
        {
            ConstAnyRef targetRef = targetData.ToRef();

            HYP_CORE_ASSERT(targetRef.HasValue(), "Invalid target reference");
            HYP_CORE_ASSERT(targetRef.GetTypeId() != TypeId::Void(), "Invalid target type");

            const UIntPtr baseAddress = reinterpret_cast<UIntPtr>(targetRef.GetPointer());
            HYP_CORE_ASSERT(baseAddress != 0, "Invalid target base address");

            const UIntPtr memberAddress = baseAddress + offset;
            HYP_CORE_ASSERT(memberAddress != 0, "Invalid member address");

            return *reinterpret_cast<const HypData*>(memberAddress);
        };

        m_setProc = [offset](HypData& targetData, const HypData& data) -> void
        {
            AnyRef targetRef = targetData.ToRef();

            HYP_CORE_ASSERT(targetRef.HasValue(), "Invalid target reference");
            HYP_CORE_ASSERT(targetRef.GetTypeId() != TypeId::Void(), "Invalid target type");

            const UIntPtr baseAddress = reinterpret_cast<UIntPtr>(targetRef.GetPointer());
            HYP_CORE_ASSERT(baseAddress != 0, "Invalid target base address");

            const UIntPtr memberAddress = baseAddress + offset;
            HYP_CORE_ASSERT(memberAddress != 0, "Invalid member address");

            *reinterpret_cast<HypData*>(memberAddress) = data;
        };

        // @TODO: Serialize/Deserialize
    }

    template <class ThisType, class FieldType>
    HypField(Name name, FieldType ThisType::* member, uint32 offset, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeId(TypeId::ForType<FieldType>()),
          m_targetTypeId(TypeId::ForType<ThisType>()),
          m_offset(offset),
          m_size(sizeof(FieldType)),
          m_attributes(attributes)
    {
        m_getProc = [member](const HypData& targetData) -> HypData
        {
            if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>)
            {
                HYP_FAIL("Cannot assign non-copy-assignable field");
            }
            else
            {
                HYP_CORE_ASSERT(targetData.Is<ThisType>(), "Invalid target type: Expected %s (TypeId: %u), but got TypeId: %u",
                    TypeName<ThisType>().Data(), TypeId::ForType<ThisType>().Value(), targetData.GetTypeId().Value());

                decltype(auto) target = targetData.Get<ThisType>();

                return HypData(target.*member);
            }
        };

        m_setProc = [member](HypData& targetData, const HypData& data) -> void
        {
            if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>)
            {
                HYP_FAIL("Cannot deserialize non-copy-assignable field");
            }
            else
            {
                HYP_CORE_ASSERT(targetData.Is<ThisType>(), "Invalid target type: Expected %s (TypeId: %u), but got TypeId: %u",
                    TypeName<ThisType>().Data(), TypeId::ForType<ThisType>().Value(), targetData.GetTypeId().Value());

                decltype(auto) target = targetData.Get<ThisType>();

                if constexpr (std::is_array_v<NormalizedType<FieldType>>)
                {
                    using InnerType = std::remove_extent_t<NormalizedType<FieldType>>;

                    if (data.IsNull())
                    {
                        for (SizeType i = 0; i < std::extent_v<NormalizedType<FieldType>>; i++)
                        {
                            (target.*member)[i] = NormalizedType<InnerType> {};
                        }
                    }
                    else
                    {
                        auto& arrayValue = data.Get<NormalizedType<FieldType>>();

                        for (SizeType i = 0; i < arrayValue.Size(); i++)
                        {
                            (target.*member)[i] = arrayValue[i];
                        }
                    }
                }
                else
                {
                    if (data.IsNull())
                    {
                        target.*member = NormalizedType<FieldType> {};
                    }
                    else
                    {
                        target.*member = data.Get<NormalizedType<FieldType>>();
                    }
                }
            }
        };

        if (m_attributes["serialize"] || m_attributes["xmlattribute"])
        {
            m_serializeProc = [member](const HypData& targetData, EnumFlags<FBOMDataFlags> flags) -> FBOMData
            {
                FBOMData out;

                if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>)
                {
                    HYP_FAIL("Cannot serialize non-copy-assignable field");
                }
                else
                {
                    HYP_CORE_ASSERT(targetData.Is<ThisType>(), "Invalid target type: Expected %s (TypeId: %u), but got TypeId: %u",
                        TypeName<ThisType>().Data(), TypeId::ForType<ThisType>().Value(), targetData.GetTypeId().Value());

                    decltype(auto) target = targetData.Get<ThisType>();

                    if (FBOMResult err = HypDataHelper<NormalizedType<FieldType>>::Serialize(target.*member, out, flags))
                    {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }
                }

                return out;
            };

            m_deserializeProc = [member](FBOMLoadContext& context, HypData& targetData, const FBOMData& data) -> Result
            {
                if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>)
                {
                    return HYP_MAKE_ERROR(Error, "Cannot deserialize non-copy-assignable field");
                }
                else
                {
                    if (!targetData.Is<ThisType>())
                    {
                        return HYP_MAKE_ERROR(Error, "Invalid target type: Expected {} (TypeId: {}), but got TypeId: {}",
                            TypeName<ThisType>().Data(), TypeId::ForType<ThisType>().Value(), targetData.GetTypeId().Value());
                    }

                    HypData value;

                    if (FBOMResult err = HypDataHelper<NormalizedType<FieldType>>::Deserialize(context, data, value))
                    {
                        return HYP_MAKE_ERROR(Error, "Failed to deserialize data: {}", err.message);
                    }

                    decltype(auto) target = targetData.Get<ThisType>();

                    if constexpr (std::is_array_v<NormalizedType<FieldType>>)
                    {
                        using InnerType = std::remove_extent_t<NormalizedType<FieldType>>;

                        if (value.IsNull())
                        {
                            for (SizeType i = 0; i < std::extent_v<NormalizedType<FieldType>>; i++)
                            {
                                (target.*member)[i] = NormalizedType<InnerType> {};
                            }
                        }
                        else
                        {
                            auto& arrayValue = value.Get<NormalizedType<FieldType>>();

                            for (SizeType i = 0; i < arrayValue.Size(); i++)
                            {
                                (target.*member)[i] = arrayValue[i];
                            }
                        }
                    }
                    else
                    {
                        if (value.IsNull())
                        {
                            target.*member = NormalizedType<FieldType> {};
                        }
                        else
                        {
                            target.*member = value.Get<NormalizedType<FieldType>>();
                        }
                    }
                }

                return {};
            };
        }
    }

    HypField(const HypField& other) = delete;
    HypField& operator=(const HypField& other) = delete;

    HypField(HypField&& other) noexcept = default;
    HypField& operator=(HypField&& other) noexcept = default;

    virtual ~HypField() override = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_FIELD;
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
        return m_targetTypeId;
    }

    virtual bool CanSerialize() const override
    {
        return IsValid() && m_serializeProc.IsValid();
    }

    virtual bool CanDeserialize() const override
    {
        return IsValid() && m_deserializeProc.IsValid();
    }

    HYP_FORCE_INLINE bool Serialize(const HypData& target, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const
    {
        return Serialize(Span<HypData>(&const_cast<HypData&>(target), 1), out, flags);
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

        out = m_serializeProc(*args.Data(), flags);

        return true;
    }

    virtual bool Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& data) const override
    {
        if (!CanDeserialize())
        {
            return false;
        }

        return bool(m_deserializeProc(context, target, data));
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

    HYP_FORCE_INLINE uint32 GetOffset() const
    {
        return m_offset;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE HypData Get(const HypData& targetData) const
    {
        return m_getProc(targetData);
    }

    HYP_FORCE_INLINE void Set(HypData& targetData, const HypData& data) const
    {
        return m_setProc(targetData, data);
    }

private:
    Name m_name;
    TypeId m_typeId;
    TypeId m_targetTypeId;
    uint32 m_offset;
    uint32 m_size;

    HypClassAttributeSet m_attributes;

    Proc<HypData(const HypData&)> m_getProc;
    Proc<void(HypData&, const HypData&)> m_setProc;

    Proc<FBOMData(const HypData&, EnumFlags<FBOMDataFlags> flags)> m_serializeProc;
    Proc<Result(FBOMLoadContext&, HypData&, const FBOMData&)> m_deserializeProc;
};

} // namespace hyperion
