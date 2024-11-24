/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Scene.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

namespace hyperion {
namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, ScenesBuffer, 1, sizeof(SceneShaderData), true);

} // namespace renderer
} // namespace hyperion
