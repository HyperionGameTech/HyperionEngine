/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Camera.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

namespace hyperion {
namespace renderer {

HYP_DESCRIPTOR_CBUFF(Scene, CamerasBuffer, 1, sizeof(CameraShaderData), true);

} // namespace renderer
} // namespace hyperion