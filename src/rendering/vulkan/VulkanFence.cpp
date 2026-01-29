/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/Device.hpp>

#define DEFAULT_FENCE_TIMEOUT 100000000000

#include <VulkanFence.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

VulkanFence::VulkanFence()
    : m_handle(VK_NULL_HANDLE),
      m_lastFrameResult(VK_SUCCESS)
{
}

VulkanFence::~VulkanFence()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFence(g_renderInterface->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }
}

void VulkanFence::Create(bool createSignaled)
{
    Assert(m_handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    if (createSignaled)
    {
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VkResult result = vkCreateFence(g_renderInterface->GetDevice()->GetDevice(), &fenceCreateInfo, nullptr, &m_handle);
    Assert(result == VK_SUCCESS, "Failed to create Vulkan fence, VkResult: {}", result);
}

void VulkanFence::Wait(bool timeoutLoop)
{
    Assert(m_handle != VK_NULL_HANDLE);

    VkResult result = VK_SUCCESS;

    do
    {
        result = vkWaitForFences(g_renderInterface->GetDevice()->GetDevice(), 1, &m_handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    while (result == VK_TIMEOUT && timeoutLoop);

    m_lastFrameResult = result;

    Assert(result == VK_SUCCESS, "Failed to wait for Vulkan fence, VkResult: {}", result);
}

void VulkanFence::Reset()
{
    VkResult result = vkResetFences(g_renderInterface->GetDevice()->GetDevice(), 1, &m_handle);
    Assert(result == VK_SUCCESS, "Failed to reset Vulkan fence, VkResult: {}", result);
}

} // namespace Hyperion
