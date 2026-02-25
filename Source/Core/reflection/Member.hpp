/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Name.hpp>

/// We need to include containers for TypeInfo traits.
#include <Core/containers/String.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/containers/HashSet.hpp>
#include <Core/containers/Array.hpp>

#include <Core/utilities/StringView.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>
#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Result.hpp>

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
