/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/Device.hpp>

#define DEFAULT_FENCE_TIMEOUT 100000000000

#include <VulkanFence.generated.inl>

namespace Hyperion {

extern VulkanRenderBackend* g_renderBackend;

VulkanFence::VulkanFence()
    : m_handle(VK_NULL_HANDLE),
      m_lastFrameResult(VK_SUCCESS)
{
}

VulkanFence::~VulkanFence()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFence(g_renderBackend->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }
}

RendererResult VulkanFence::Create()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VULKAN_CHECK(vkCreateFence(g_renderBackend->GetDevice()->GetDevice(), &fenceCreateInfo, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::Wait(bool timeoutLoop)
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VkResult vkResult;

    do
    {
        vkResult = vkWaitForFences(g_renderBackend->GetDevice()->GetDevice(), 1, &m_handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    while (vkResult == VK_TIMEOUT && timeoutLoop);

    m_lastFrameResult = vkResult;

    VULKAN_CHECK(vkResult);

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::Reset()
{
    VULKAN_CHECK(vkResetFences(g_renderBackend->GetDevice()->GetDevice(), 1, &m_handle));

    HYPERION_RETURN_OK;
}

} // namespace Hyperion
