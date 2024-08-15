/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_FIELD_HPP
#define HYPERION_CORE_HYP_FIELD_HPP

#include <core/object/HypData.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/Serialization.hpp>
#include <asset/serialization/SerializationWrapper.hpp>
#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

struct HypField
{
    Name    name;
    TypeID  type_id;
    uint32  offset;
    uint32  size;

    HypField()
        : name(Name::Invalid()),
          type_id(TypeID::Void()),
          offset(0),
          size(0)
    {
    }

    template <class ThisType, class FieldType>
    HypField(Name name, FieldType ThisType::*member, uint32 offset)
        : name(name),
          type_id(TypeID::ForType<FieldType>()),
          offset(offset),
          size(sizeof(FieldType))
    {
    }

    HypField(const HypField &other)                 = delete;
    HypField &operator=(const HypField &other)      = delete;
    HypField(HypField &&other) noexcept             = default;
    HypField &operator=(HypField &&other) noexcept  = default;
    ~HypField()                                     = default;
};

} // namespace hyperion

#endif