#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <vulkan/vulkan.h>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Pipeline;

template <PlatformType PLATFORM>
class Device;

template <>
class Fence<Platform::VULKAN>
{
public:
    Fence(bool create_signaled = false);
    Fence(const Fence &other) = delete;
    Fence &operator=(const Fence &other) = delete;
    ~Fence();

    VkFence &GetHandle() { return m_handle; }
    const VkFence &GetHandle() const { return m_handle; }

    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);
    Result WaitForGPU(Device<Platform::VULKAN> *device, bool timeout_loop = false, VkResult *out_result = nullptr);
    Result Reset(Device<Platform::VULKAN> *device);

private:
    VkFence m_handle;
    bool m_create_signaled;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
