/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_FIELD_HPP
#define HYPERION_CORE_HYP_FIELD_HPP

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/containers/String.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/serialization/fbom/FBOMData.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

class HypField : public IHypMember
{
public:
    HypField(Span<const HypClassAttribute> attributes = { })
        : m_name(Name::Invalid()),
          m_type_id(TypeID::Void()),
          m_target_type_id(TypeID::Void()),
          m_offset(~0u),
          m_size(0),
          m_attributes(attributes)
    {
    }

    template <class ThisType, class FieldType>
    HypField(Name name, FieldType ThisType::*member, uint32 offset, Span<const HypClassAttribute> attributes = { })
        : m_name(name),
          m_type_id(TypeID::ForType<FieldType>()),
          m_target_type_id(TypeID::ForType<ThisType>()),
          m_offset(offset),
          m_size(sizeof(FieldType)),
          m_attributes(attributes)
    {
        m_get_proc = [member](const HypData &target_data) -> HypData
        {
            if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                HYP_FAIL("Cannot assign non-copy-assignable field");
            } else {
                ConstAnyRef target_ref = target_data.ToRef();

                AssertThrow(target_ref.HasValue());
                AssertThrowMsg(target_ref.Is<ThisType>(), "Invalid target type: Expected %s (TypeID: %u), but got TypeID: %u",
                    TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());

                return HypData(static_cast<const ThisType *>(target_ref.GetPointer())->*member);
            }
        };

        m_set_proc = [member](HypData &target_data, const HypData &data) -> void
        {
            if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                HYP_FAIL("Cannot deserialize non-copy-assignable field");
            } else {
                AnyRef target_ref = target_data.ToRef();

                AssertThrow(target_ref.HasValue());
                AssertThrowMsg(target_ref.Is<ThisType>(), "Invalid target type: Expected %s (TypeID: %u), but got TypeID: %u",
                    TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());

                ThisType *target = static_cast<ThisType *>(target_ref.GetPointer());

                if constexpr (std::is_array_v<NormalizedType<FieldType>>) {
                    using InnerType = std::remove_extent_t<NormalizedType<FieldType>>;

                    if (data.IsNull()) {
                        for (SizeType i = 0; i < std::extent_v<NormalizedType<FieldType>>; i++) {
                            (target->*member)[i] = NormalizedType<InnerType> { };
                        }
                    } else {
                        auto &array_value = data.Get<NormalizedType<FieldType>>();

                        for (SizeType i = 0; i < array_value.Size(); i++) {
                            (target->*member)[i] = array_value[i];
                        }
                    }
                } else {
                    if (data.IsNull()) {
                        target->*member = NormalizedType<FieldType> { };
                    } else {
                        target->*member = data.Get<NormalizedType<FieldType>>();
                    }
                }
            }
        };

        if (m_attributes["serialize"] || m_attributes["xmlattribute"]) {
            m_serialize_proc = [member](const HypData &target_data) -> fbom::FBOMData
            {
                if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                    HYP_FAIL("Cannot serialize non-copy-assignable field");
                } else {
                    ConstAnyRef target_ref = target_data.ToRef();

                    AssertThrow(target_ref.HasValue());
                    AssertThrowMsg(target_ref.Is<ThisType>(), "Invalid target type: Expected %s (TypeID: %u), but got TypeID: %u",
                        TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());

                    fbom::FBOMData out;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<FieldType>>::Serialize(static_cast<const ThisType *>(target_ref.GetPointer())->*member, out)) {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            m_deserialize_proc = [member](HypData &target_data, const fbom::FBOMData &data) -> Result
            {
                if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                    return HYP_MAKE_ERROR(Error, "Cannot deserialize non-copy-assignable field");
                } else {
                    AnyRef target_ref = target_data.ToRef();

                    if (!target_ref.HasValue()) {
                        return HYP_MAKE_ERROR(Error, "Invalid target reference");
                    }

                    if (!target_ref.Is<ThisType>()) {
                        return HYP_MAKE_ERROR(Error, "Invalid target type: Expected {} (TypeID: {}), but got TypeID: {}",
                            TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());
                    }

                    HypData value;
        
                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<FieldType>>::Deserialize(data, value)) {
                        return HYP_MAKE_ERROR(Error, "Failed to deserialize data: {}", err.message);
                    }

                    ThisType *target = static_cast<ThisType *>(target_ref.GetPointer());

                    if constexpr (std::is_array_v<NormalizedType<FieldType>>) {
                        using InnerType = std::remove_extent_t<NormalizedType<FieldType>>;

                        if (value.IsNull()) {
                            for (SizeType i = 0; i < std::extent_v<NormalizedType<FieldType>>; i++) {
                                (target->*member)[i] = NormalizedType<InnerType> { };
                            }
                        } else {
                            auto &array_value = value.Get<NormalizedType<FieldType>>();

                            for (SizeType i = 0; i < array_value.Size(); i++) {
                                (target->*member)[i] = array_value[i];
                            }
                        }
                    } else {
                        if (value.IsNull()) {
                            target->*member = NormalizedType<FieldType> { };
                        } else {
                            target->*member = value.Get<NormalizedType<FieldType>>();
                        }
                    }
                }

                return { };
            };
        }
    }

    HypField(const HypField &other)                 = delete;
    HypField &operator=(const HypField &other)      = delete;
    HypField(HypField &&other) noexcept             = default;
    HypField &operator=(HypField &&other) noexcept  = default;
    virtual ~HypField() override                    = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_FIELD;
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
        return m_target_type_id;
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

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_name.IsValid()
            && m_type_id != TypeID::Void()
            && m_size != 0;
    }

    HYP_FORCE_INLINE uint32 GetOffset() const
        { return m_offset; }

    HYP_FORCE_INLINE bool CanSerialize() const
        { return IsValid() && m_serialize_proc.IsValid(); }

    HYP_FORCE_INLINE fbom::FBOMData Serialize(const HypData &target_data) const
    {
        AssertThrow(CanSerialize());

        return m_serialize_proc(target_data);
    }

    HYP_FORCE_INLINE bool CanDeserialize() const
        { return IsValid() && m_deserialize_proc.IsValid(); }

    HYP_FORCE_INLINE Result Deserialize(HypData &target_data, const fbom::FBOMData &data) const
    {
        if (!CanDeserialize()) {
            return HYP_MAKE_ERROR(Error, "Cannot deserialize field");
        }

        return m_deserialize_proc(target_data, data);
    }

    HYP_FORCE_INLINE HypData Get(const HypData &target_data) const
    {
        return m_get_proc(target_data);
    }

    HYP_FORCE_INLINE void Set(HypData &target_data, const HypData &data) const
    {
        return m_set_proc(target_data, data);
    }

private:
    Name                                                    m_name;
    TypeID                                                  m_type_id;
    TypeID                                                  m_target_type_id;
    uint32                                                  m_offset;
    uint32                                                  m_size;

    HypClassAttributeSet                                    m_attributes;

    Proc<HypData, const HypData &>                          m_get_proc;
    Proc<void, HypData &, const HypData &>                  m_set_proc;

    Proc<fbom::FBOMData, const HypData &>                   m_serialize_proc;
    Proc<Result, HypData &, const fbom::FBOMData &>   m_deserialize_proc;
};

} // namespace hyperion

#endif