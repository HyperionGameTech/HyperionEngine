/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererHelpers.hpp>

#include <math/MathUtil.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

uint MipmapSize(uint src_size, int lod)
{
    return MathUtil::Max(src_size >> lod, 1u);
}

} // namespace helpers

namespace platform {

} // namespace platform

} // namespace renderer
} // namespace hyperion