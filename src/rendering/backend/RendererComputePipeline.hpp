/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_COMPUTE_PIPELINE_H
#define HYPERION_V2_BACKEND_RENDERER_COMPUTE_PIPELINE_H

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererPipeline.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class ComputePipeline : public Pipeline<PLATFORM>
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using ComputePipeline = platform::ComputePipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif