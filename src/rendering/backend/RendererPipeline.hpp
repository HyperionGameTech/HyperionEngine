#ifndef HYPERION_V2_BACKEND_RENDERER_PIPELINE_H
#define HYPERION_V2_BACKEND_RENDERER_PIPELINE_H

#include <rendering/backend/Platform.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Pipeline { };

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Pipeline = platform::Pipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif