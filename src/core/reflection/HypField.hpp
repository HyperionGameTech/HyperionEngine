/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/HypData.hpp>
#include <core/reflection/HypClassAttribute.hpp>
#include <core/reflection/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/reflection/TypeId.hpp>
#include <core/reflection/TypeInfoFwd.hpp>
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
          m_typeInfo(&TypeInfo_Void()),
          m_targetTypeInfo(&TypeInfo_Void()),
          m_offset(~0u),
          m_size(0),
          m_attributes(attributes)
    {
    }

    /*! \brief Script object (HypObjectBase) overload */
    HypField(Name name, const TypeInfo* typeInfo, const TypeInfo* targetTypeInfo, uint32 offset, uint32 size, const Span<const HypClassAttribute>& attributes = {})
        : m_name(name),
          m_typeInfo(typeInfo),
          m_targetTypeInfo(targetTypeInfo),
          m_offset(offset),
          m_size(size),
          m_attributes(attributes)
    {
        HYP_CORE_ASSERT(m_typeInfo != nullptr, "TypeInfo cannot be null");
        HYP_CORE_ASSERT(m_targetTypeInfo != nullptr, "Target TypeInfo cannot be null");

        HYP_CORE_ASSERT(TypeInfo_GetId(*typeInfo) == TypeId::ForType<HypData>(), "HypField must be HypData for script objects");

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
          m_typeInfo(&TypeInfo_ForType<FieldType>()),
          m_targetTypeInfo(&TypeInfo_ForType<ThisType>()),
          m_offset(offset),
          m_size(sizeof(FieldType)),
          m_attributes(attributes)
    {
        if constexpr (IsDelegateV<NormalizedType<FieldType>>)
        {
            m_flags |= HypMemberFlags::DELEGATE;
        }

        m_getProc = [member](const HypData& targetData) -> HypData
        {
            decltype(auto) target = targetData.Get<ThisType>();

#if 0
            // Use HypDataArray wrapper type for containers so we can iterate over them generically.
            // Skip doing this for strings though, as they are also containers but should be treated as a single value.
            if constexpr (std::is_base_of_v<IContainer, NormalizedType<FieldType>> && !IsStringV<NormalizedType<FieldType>>)
            {
                // Containers are always returned as a reference to avoid copies
                return HypData(GenericArrayWrapper(GenericArrayWrapper::AS_REFERENCE, (target.*member)));
            }
            else
            {
                return HypData(AnyRef(&(target.*member)));
            }
#else
            if constexpr (IsDelegateV<NormalizedType<FieldType>>)
            {
                // special handling for delegate fields: always return reference instead of value.
                return HypData(AnyRef(&(target.*member)));
            }
            else
            {
                return HypData(target.*member);
            }
#endif
        };

        m_setProc = [member](HypData& targetData, const HypData& data) -> void
        {
            if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>)
            {
                HYP_FAIL("Cannot set non-copy-assignable field");
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

        m_serializeProc = [member](const HypData& targetData, EnumFlags<FBOMDataFlags> flags, FBOMData& outData) -> Result
        {
            decltype(auto) target = targetData.Get<ThisType>();

            if (FBOMResult err = HypDataHelper<NormalizedType<FieldType>>::Serialize(target.*member, outData, flags))
            {
                return HYP_MAKE_ERROR(Error, "Failed to serialize data: {}", err.message);
            }

            return {};
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

    virtual const TypeInfo& GetTypeInfo() const override
    {
        return *m_typeInfo;
    }

    virtual const TypeInfo& GetTargetTypeInfo() const override
    {
        return *m_targetTypeInfo;
    }

    virtual bool CanSerialize() const override
    {
        return IsValid() && m_serializeProc.IsValid();
    }

    virtual bool CanDeserialize() const override
    {
        return IsValid() && m_deserializeProc.IsValid();
    }

    virtual Result Serialize(Span<HypData> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const override
    {
        if (!CanSerialize())
        {
            return HYP_MAKE_ERROR(Error, "Field cannot be serialized");
        }

        if (args.Size() != 1)
        {
            return HYP_MAKE_ERROR(Error, "Expected exactly one argument to serialize field, got {}", args.Size());
        }

        return m_serializeProc(*args.Data(), flags, out);
    }

    virtual Result Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& in) const override
    {
        if (!CanDeserialize())
        {
            return HYP_MAKE_ERROR(Error, "Field cannot be deserialized");
        }

        return m_deserializeProc(context, target, in);
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
            && m_typeInfo != nullptr && TypeInfo_GetId(*m_typeInfo) != TypeId::Void()
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
    const TypeInfo* m_typeInfo;
    const TypeInfo* m_targetTypeInfo;
    uint32 m_offset;
    uint32 m_size;

    HypClassAttributeSet m_attributes;

    Proc<HypData(const HypData&)> m_getProc;
    Proc<void(HypData&, const HypData&)> m_setProc;

    Proc<Result(const HypData& target, EnumFlags<FBOMDataFlags> flags, FBOMData& outData)> m_serializeProc;
    Proc<Result(FBOMLoadContext& context, HypData& target, const FBOMData& inData)> m_deserializeProc;
};

} // namespace hyperion
