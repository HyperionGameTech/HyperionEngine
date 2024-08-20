/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>

#include <core/object/HypClass.hpp>

namespace hyperion::fbom {

FBOMObjectType::FBOMObjectType(const HypClass *hyp_class)
    : FBOMType(
          hyp_class->GetName().LookupString(),
          hyp_class->GetSize(),
          hyp_class->GetTypeID(),
          FBOMTypeFlags::CONTAINER,
          hyp_class->GetParent() ? FBOMObjectType(hyp_class->GetParent()) : FBOMBaseObjectType()
      )
{
}

} // namespace hyperion::fbom
