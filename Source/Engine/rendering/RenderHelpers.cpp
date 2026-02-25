/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderHelpers.hpp>

#include <Core/math/MathUtil.hpp>

namespace Hyperion {
namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod)
{
    return MathUtil::Max(srcSize >> lod, 1u);
}

} // namespace helpers

} // namespace Hyperion
