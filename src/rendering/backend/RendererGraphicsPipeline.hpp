#ifndef HYPERION_V2_BACKEND_RENDERER_GRAPHICS_PIPELINE_H
#define HYPERION_V2_BACKEND_RENDERER_GRAPHICS_PIPELINE_H

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererPipeline.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class GraphicsPipeline : public Pipeline<PLATFORM>
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using GraphicsPipeline = platform::GraphicsPipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif