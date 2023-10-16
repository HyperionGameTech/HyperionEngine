#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP

#include <rendering/backend/RendererDevice.hpp>

#include <vulkan/vulkan.h>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {
namespace renderer {

class Fence {
public:
    Fence(bool create_signaled = false);
    Fence(const Fence &other) = delete;
    Fence &operator=(const Fence &other) = delete;
    ~Fence();

    VkFence &GetHandle() { return m_handle; }
    const VkFence &GetHandle() const { return m_handle; }

    Result Create(Device *device);
    Result Destroy(Device *device);
    Result WaitForGPU(Device *device, bool timeout_loop = false, VkResult *out_result = nullptr);
    Result Reset(Device *device);

private:
    VkFence m_handle;
    bool m_create_signaled;
};

} // namespace renderer
} // namespace hyperion

#endif
