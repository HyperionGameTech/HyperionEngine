/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FEATURES_HPP
#define HYPERION_BACKEND_RENDERER_FEATURES_HPP

#include <core/Defines.hpp>

namespace hyperion {

} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFeatures.hpp>
#else
#error Unsupported rendering backend
#endif

#endif