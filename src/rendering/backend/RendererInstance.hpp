#ifndef HYPERION_V2_BACKEND_RENDERER_INSTANCE_H
#define HYPERION_V2_BACKEND_RENDERER_INSTANCE_H

#include <rendering/backend/Platform.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {
template <PlatformType PLATFORM>
class Instance
{

};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererInstance.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Instance = platform::Instance<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif