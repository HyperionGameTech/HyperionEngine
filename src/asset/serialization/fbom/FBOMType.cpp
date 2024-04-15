/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>

namespace hyperion::v2::fbom {

FBOMType FBOMType::Extend(const FBOMType &object) const
{
    return FBOMObjectType(object.name, *this);
}

} // namespace hyperion::v2::fbom
