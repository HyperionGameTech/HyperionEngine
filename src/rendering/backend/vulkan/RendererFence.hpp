#ifndef HYPERION_RENDERER_FENCE_H
#define HYPERION_RENDERER_FENCE_H

#include "RendererDevice.hpp"

#include <vulkan/vulkan.h>

#define DEFAULT_FENCE_TIMEOUT 10000000

namespace hyperion {
namespace renderer {

class Fence {
public:
    Fence(bool create_signaled = false);
    Fence(const Fence &other) = delete;
    Fence &operator=(const Fence &other) = delete;
    ~Fence();

    inline VkFence &GetHandle()             { return m_handle; }
    inline const VkFence &GetHandle() const { return m_handle; }

    Result Create(Device *device);
    Result Destroy(Device *device);
    Result WaitForGpu(Device *device, bool timeout_loop = false, VkResult *out_result = nullptr);
    Result Reset(Device *device);

private:
    VkFence m_handle;
    bool m_create_signaled;
};

} // namespace renderer
} // namespace hyperion

#endif
