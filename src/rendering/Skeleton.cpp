/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Skeleton.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

namespace hyperion {
namespace renderer {

HYP_DESCRIPTOR_SSBO(Object, SkeletonsBuffer, 1, sizeof(SkeletonShaderData), true);

} // namespace renderer
} // namespace hyperion