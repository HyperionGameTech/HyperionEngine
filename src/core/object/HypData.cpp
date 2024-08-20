/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

#include <core/ObjectPool.hpp>

#include <core/utilities/Format.hpp>

#include <Engine.hpp>

namespace hyperion {

fbom::FBOMResult HypDataMarshalHelper::NoMarshalRegistered(ANSIStringView type_name)
{
    return fbom::FBOMResult { fbom::FBOMResult::FBOM_ERR, HYP_FORMAT("No marshal registered for {}", type_name) };
}

} // namespace hyperion