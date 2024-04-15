/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererDevice.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

Fence<Platform::VULKAN>::Fence()
    : m_handle(VK_NULL_HANDLE)
{
}

Fence<Platform::VULKAN>::~Fence()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "fence should have been destroyed");
}

Result Fence<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fence_create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    HYPERION_VK_CHECK(vkCreateFence(device->GetDevice(), &fence_create_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result Fence<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_handle != VK_NULL_HANDLE);

    vkDestroyFence(device->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

Result Fence<Platform::VULKAN>::WaitForGPU(Device<Platform::VULKAN> *device, bool timeout_loop, VkResult *out_result)
{
    AssertThrow(m_handle != VK_NULL_HANDLE);

    VkResult vk_result;

    do {
        vk_result = vkWaitForFences(device->GetDevice(), 1, &m_handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    } while (vk_result == VK_TIMEOUT && timeout_loop);

    if (out_result != nullptr) {
        *out_result = vk_result;
    }

    HYPERION_VK_CHECK(vk_result);

    HYPERION_RETURN_OK;
}

Result Fence<Platform::VULKAN>::Reset(Device<Platform::VULKAN> *device)
{
    HYPERION_VK_CHECK(vkResetFences(device->GetDevice(), 1, &m_handle));

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion