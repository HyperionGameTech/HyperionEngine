/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_FEATURES_H
#define HYPERION_V2_BACKEND_RENDERER_FEATURES_H

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

enum class ImageSupportType
{
    SRV,
    UAV,
    DEPTH
};

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFeatures.hpp>
#else
#error Unsupported rendering backend
#endif

#endif