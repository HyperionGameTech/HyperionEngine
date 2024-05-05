/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererDevice.hpp>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {
namespace renderer {
namespace platform {

template <>
Fence<Platform::VULKAN>::Fence()
    : m_platform_impl { this }
{
}

template <>
Fence<Platform::VULKAN>::~Fence()
{
    AssertThrowMsg(m_platform_impl.handle == VK_NULL_HANDLE, "fence should have been destroyed");
}

template <>
Result Fence<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_platform_impl.handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fence_create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    HYPERION_VK_CHECK(vkCreateFence(device->GetDevice(), &fence_create_info, nullptr, &m_platform_impl.handle));

    HYPERION_RETURN_OK;
}

template <>
Result Fence<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (m_platform_impl.handle != VK_NULL_HANDLE) {
        vkDestroyFence(device->GetDevice(), m_platform_impl.handle, nullptr);
        m_platform_impl.handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

template <>
Result Fence<Platform::VULKAN>::WaitForGPU(Device<Platform::VULKAN> *device, bool timeout_loop)
{
    AssertThrow(m_platform_impl.handle != VK_NULL_HANDLE);

    VkResult vk_result;

    do {
        vk_result = vkWaitForFences(device->GetDevice(), 1, &m_platform_impl.handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    } while (vk_result == VK_TIMEOUT && timeout_loop);

    HYPERION_VK_CHECK(vk_result);

    m_platform_impl.last_frame_result = vk_result;

    HYPERION_RETURN_OK;
}

template <>
Result Fence<Platform::VULKAN>::Reset(Device<Platform::VULKAN> *device)
{
    HYPERION_VK_CHECK(vkResetFences(device->GetDevice(), 1, &m_platform_impl.handle));

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion