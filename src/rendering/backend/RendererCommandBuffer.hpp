#ifndef HYPERION_V2_BACKEND_RENDERER_COMMAND_BUFFER_H
#define HYPERION_V2_BACKEND_RENDERER_COMMAND_BUFFER_H

#include <rendering/backend/Platform.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {

enum CommandBufferType
{
    COMMAND_BUFFER_PRIMARY,
    COMMAND_BUFFER_SECONDARY
};

namespace platform {

template <PlatformType PLATFORM>
class CommandBuffer { };

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using CommandBuffer = platform::CommandBuffer<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif