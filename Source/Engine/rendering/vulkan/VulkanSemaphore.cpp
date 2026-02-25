/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/Device.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <VulkanSemaphore.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern VulkanRenderInterface* g_renderInterface;

VulkanSemaphore::VulkanSemaphore()
    : m_handle(VK_NULL_HANDLE)
{
}

VulkanSemaphore::~VulkanSemaphore()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroySemaphore(g_renderInterface->GetDevice()->GetDevice(), handle, nullptr);
            }));

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
        vkCreateSemaphore(g_renderInterface->GetDevice()->GetDevice(), &semaphoreInfo, nullptr, &m_handle),
        "Failed to create semaphore");

    return {};
}

} // namespace Hyperion
