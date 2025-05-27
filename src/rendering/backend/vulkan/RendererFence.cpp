/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererDevice.hpp>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {

extern IRenderingAPI* g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI* GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI*>(g_rendering_api);
}

VulkanFence::VulkanFence()
    : m_handle(VK_NULL_HANDLE),
      m_last_frame_result(VK_SUCCESS)
{
}

VulkanFence::~VulkanFence()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "fence should have been destroyed");
}

RendererResult VulkanFence::Create()
{
    AssertThrow(m_handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fence_create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    HYPERION_VK_CHECK(vkCreateFence(GetRenderingAPI()->GetDevice()->GetDevice(), &fence_create_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::Destroy()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFence(GetRenderingAPI()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::WaitForGPU(bool timeout_loop)
{
    AssertThrow(m_handle != VK_NULL_HANDLE);

    VkResult vk_result;

    do
    {
        vk_result = vkWaitForFences(GetRenderingAPI()->GetDevice()->GetDevice(), 1, &m_handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    while (vk_result == VK_TIMEOUT && timeout_loop);

    HYPERION_VK_CHECK(vk_result);

    m_last_frame_result = vk_result;

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::Reset()
{
    HYPERION_VK_CHECK(vkResetFences(GetRenderingAPI()->GetDevice()->GetDevice(), 1, &m_handle));

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion