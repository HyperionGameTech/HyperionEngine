/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>

#include <rendering/RenderDevice.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanFence::VulkanFence()
    : m_handle(VK_NULL_HANDLE),
      m_lastFrameResult(VK_SUCCESS)
{
}

VulkanFence::~VulkanFence()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFence(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }
}

RendererResult VulkanFence::Create()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VULKAN_CHECK(vkCreateFence(GetRenderBackend()->GetDevice()->GetDevice(), &fenceCreateInfo, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::WaitForGPU(bool timeoutLoop)
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VkResult vkResult;

    do
    {
        vkResult = vkWaitForFences(GetRenderBackend()->GetDevice()->GetDevice(), 1, &m_handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    while (vkResult == VK_TIMEOUT && timeoutLoop);

    VULKAN_CHECK(vkResult);

    m_lastFrameResult = vkResult;

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::Reset()
{
    VULKAN_CHECK(vkResetFences(GetRenderBackend()->GetDevice()->GetDevice(), 1, &m_handle));

    HYPERION_RETURN_OK;
}

} // namespace hyperion
