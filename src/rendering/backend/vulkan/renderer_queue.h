#ifndef HYPERION_RENDERER_QUEUE_H
#define HYPERION_RENDERER_QUEUE_H

namespace hyperion {
namespace renderer {

struct Queue {
    uint32_t      family;
    VkQueue       queue;
    VkCommandPool command_pool;
};

} // namespace renderer
} // namespace hyperion

#endif
