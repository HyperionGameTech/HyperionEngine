/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMBaseTypes.hpp>

#include <core/object/HypClass.hpp>

namespace hyperion::serialization {

FBOMObjectType::FBOMObjectType(const HypClass* hypClass)
    : FBOMType(
          hypClass->GetName().LookupString(),
          SizeType(-1),
          hypClass->GetTypeId(),
          FBOMTypeFlags::CONTAINER,
          hypClass->GetParent() ? FBOMObjectType(hypClass->GetParent()) : FBOMBaseObjectType())
{
}

} // namespace hyperion::serialization
