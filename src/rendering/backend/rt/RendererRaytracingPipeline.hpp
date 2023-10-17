#ifndef HYPERION_V2_BACKEND_RENDERER_RAYTRACING_PIPELINE_H
#define HYPERION_V2_BACKEND_RENDERER_RAYTRACING_PIPELINE_H

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererPipeline.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class RaytracingPipeline : public Pipeline<PLATFORM>
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/rt/RendererRaytracingPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using RaytracingPipeline = platform::RaytracingPipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif