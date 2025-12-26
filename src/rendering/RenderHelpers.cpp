/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderHelpers.hpp>

#include <core/math/MathUtil.hpp>

namespace Hyperion {
namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod)
{
    return MathUtil::Max(srcSize >> lod, 1u);
}

} // namespace helpers

} // namespace Hyperion
