/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

#include <core/object/ObjectPool.hpp>

#include <core/utilities/Format.hpp>

namespace hyperion {

FBOMResult HypDataMarshalHelper::NoMarshalRegistered(ANSIStringView type_name)
{
    return FBOMResult { FBOMResult::FBOM_ERR, HYP_FORMAT("No marshal registered for {}", type_name) };
}

} // namespace hyperion