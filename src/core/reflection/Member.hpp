/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Name.hpp>

/// We need to include containers for TypeInfo traits.
#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/Array.hpp>

#include <core/utilities/StringView.hpp>
#include <core/reflection/TypeInfoFwd.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Result.hpp>

namespace Hyperion {

class ClassAttributeSet;
struct ClassAttributeValue;
struct BoxedValue;
class Class;

enum class FBOMDataFlags : uint32;

namespace serialization {

class FBOMData;
class FBOMLoadContext;

} // namespace serialization

using serialization::FBOMData;
using serialization::FBOMLoadContext;

enum class MemberType : uint8
{
    None = 0x0,

    Field = 0x1,
    Method = 0x2,
    Property = 0x4,
    StaticField = 0x8,

    All = Field | Method | Property | StaticField
};

HYP_MAKE_ENUM_FLAGS(MemberType)

enum class MemberFlags : uint32
{
    None = 0x0,
    DelegateField = 0x1
};

HYP_MAKE_ENUM_FLAGS(MemberFlags);

class IMember
{
    friend class Class;

protected:
    IMember()
        : m_ownerClass(nullptr),
          m_flags(MemberFlags::None)
    {
    }

public:
    virtual ~IMember() = default;

    virtual MemberType GetMemberType() const = 0;

    virtual Name GetName() const = 0;

    virtual const TypeInfo& GetTypeInfo() const = 0;
    virtual const TypeInfo& GetTargetTypeInfo() const = 0;

    HYP_FORCE_INLINE const TypeId& GetTypeId() const
    {
        return TypeInfo_GetId(GetTypeInfo());
    }

    HYP_FORCE_INLINE const TypeId& GetTargetTypeId() const
    {
        return TypeInfo_GetId(GetTargetTypeInfo());
    }

    HYP_FORCE_INLINE const Class* GetOwnerClass() const
    {
        return m_ownerClass;
    }

    HYP_FORCE_INLINE EnumFlags<MemberFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE bool IsDelegate() const
    {
        return m_flags[MemberFlags::DelegateField];
    }

    virtual bool CanSerialize() const = 0;
    virtual bool CanDeserialize() const = 0;

    virtual Result Serialize(Span<BoxedValue> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const = 0;
    virtual Result Deserialize(FBOMLoadContext& context, BoxedValue& target, const FBOMData& value) const = 0;

    virtual const ClassAttributeSet& GetAttributes() const = 0;
    virtual const ClassAttributeValue& GetAttribute(StringHash key) const = 0;
    virtual const ClassAttributeValue& GetAttribute(StringHash key, const ClassAttributeValue& defaultValue) const = 0;

protected:
    const Class* m_ownerClass;
    EnumFlags<MemberFlags> m_flags;
};

} // namespace Hyperion
