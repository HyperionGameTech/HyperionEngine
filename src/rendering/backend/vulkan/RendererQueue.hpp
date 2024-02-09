#ifndef HYPERION_RENDERER_BACKEND_VULKAN_QUEUE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_QUEUE_HPP

#include <core/lib/FixedArray.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

struct DeviceQueue
{
    uint                            family;
    VkQueue                         queue;
    FixedArray<VkCommandPool, 8>    command_pools;
};

} // namespace renderer
} // namespace hyperion

#endif
