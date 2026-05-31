/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/ClassAttribute.hpp>
#include <Core/Reflection/Member.hpp>

#include <Core/Defines.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Reflection/TypeId.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Span.hpp>
#include <Core/Reflection/TypeInfoFwd.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Memory/Any.hpp>
#include <Core/Memory/AnyRef.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Class;

class StaticField : public IMember
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
        m_getProc = [value]() -> BoxedValue
        {
            return BoxedValue(value);
        };
    }

    template <class ConstantType, typename = std::enable_if_t<!std::is_reference_v<ConstantType>>>
    StaticField(Name name, const ConstantType* pValue, Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_typeInfo(&TypeOf<NormalizedType<ConstantType>>()),
          m_size(sizeof(NormalizedType<ConstantType>)),
          m_attributes(attributes)
    {
        m_getProc = [pValue]() -> BoxedValue
        {
            return BoxedValue(AnyRef(const_cast<NormalizedType<ConstantType>*>(pValue)));
        };
    }

    StaticField(const StaticField& other) = delete;
    StaticField& operator=(const StaticField& other) = delete;

    StaticField(StaticField&& other) noexcept = default;
    StaticField& operator=(StaticField&& other) noexcept = default;

    virtual ~StaticField() override = default;

    virtual MemberType GetMemberType() const override
    {
        return MemberType::StaticField;
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
            && m_size != 0
            && m_getProc.IsValid();
    }

    HYP_FORCE_INLINE bool CanGet() const
    {
        return m_getProc.IsValid();
    }

    HYP_FORCE_INLINE BoxedValue Get() const
    {
        return m_getProc();
    }

    void SetValue(const BoxedValue& value)
    {
        m_getProc = [value = value]() -> BoxedValue
        {
            return value;
        };
    }

    void SetValue(BoxedValue&& value)
    {
        m_getProc = [value = std::move(value)]() -> BoxedValue
        {
            return value;
        };
    }

private:
    Name m_name;
    const TypeInfo* m_typeInfo;
    uint32 m_size;
    ClassAttributeSet m_attributes;

    Proc<BoxedValue()> m_getProc;
};

} // namespace Hyperion
