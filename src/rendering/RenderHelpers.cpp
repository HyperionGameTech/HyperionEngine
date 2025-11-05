/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/RenderHelpers.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {
namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod)
{
    return MathUtil::Max(srcSize >> lod, 1u);
}

} // namespace helpers

} // namespace hyperion
