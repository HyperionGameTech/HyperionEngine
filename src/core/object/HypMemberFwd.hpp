/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class HypClassAttributeSet;
struct HypClassAttributeValue;
struct HypData;
class HypClass;

enum class FBOMDataFlags : uint32;

namespace serialization {

class FBOMData;
class FBOMLoadContext;

} // namespace serialization

using serialization::FBOMData;
using serialization::FBOMLoadContext;

enum class HypMemberType : uint8
{
    NONE = 0x0,
    TYPE_FIELD = 0x1,
    TYPE_METHOD = 0x2,
    TYPE_PROPERTY = 0x4,
    TYPE_CONSTANT = 0x8
};

HYP_MAKE_ENUM_FLAGS(HypMemberType)

class IHypMember
{
public:
    friend class HypClass;

    virtual ~IHypMember() = default;

    virtual HypMemberType GetMemberType() const = 0;

    virtual Name GetName() const = 0;

    virtual TypeId GetTypeId() const = 0;

    virtual TypeId GetTargetTypeId() const = 0;

    HYP_FORCE_INLINE const HypClass* GetOwnerClass() const
    {
        return m_ownerClass;
    }

    virtual bool CanSerialize() const = 0;
    virtual bool CanDeserialize() const = 0;

    virtual bool Serialize(Span<HypData> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const = 0;
    virtual bool Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& value) const = 0;

    virtual const HypClassAttributeSet& GetAttributes() const = 0;
    virtual const HypClassAttributeValue& GetAttribute(WeakName key) const = 0;
    virtual const HypClassAttributeValue& GetAttribute(WeakName key, const HypClassAttributeValue& defaultValue) const = 0;

protected:
    const HypClass* m_ownerClass;
};

} // namespace hyperion
