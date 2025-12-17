/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/RenderDevice.hpp>

#include <VulkanSemaphore.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

VulkanSemaphore::VulkanSemaphore()
    : m_handle(VK_NULL_HANDLE)
{
}

VulkanSemaphore::~VulkanSemaphore()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        HYP_LOG(RenderingBackend, Debug, "DESTROY Vulkan semaphore {}", (void*)m_handle);

        vkDestroySemaphore(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }
}

RendererResult VulkanSemaphore::Create()
{
    if (IsCreated())
    {
        return {};
    }

    VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VULKAN_CHECK_MSG(
        vkCreateSemaphore(GetRenderBackend()->GetDevice()->GetDevice(), &semaphoreInfo, nullptr, &m_handle),
        "Failed to create semaphore");

    return {};
}

} // namespace hyperion
