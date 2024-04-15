/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
    HYP_API Fence();
    Fence(const Fence &other)               = delete;
    Fence &operator=(const Fence &other)    = delete;
    HYP_API ~Fence();

    VkFence GetHandle() const
        { return m_handle; }

    HYP_API Result Create(Device<Platform::VULKAN> *device);
    HYP_API Result Destroy(Device<Platform::VULKAN> *device);
    HYP_API Result WaitForGPU(Device<Platform::VULKAN> *device, bool timeout_loop = false, VkResult *out_result = nullptr);
    HYP_API Result Reset(Device<Platform::VULKAN> *device);

private:
    VkFence m_handle;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
