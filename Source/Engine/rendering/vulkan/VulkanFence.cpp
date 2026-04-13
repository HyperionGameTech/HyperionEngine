/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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
    : handle(VK_NULL_HANDLE),
      lastFrameResult(VK_SUCCESS),
      isSubmitted(false)
{
}

VulkanFence& VulkanFence::operator=(VulkanFence&& other) noexcept
{
    if (this != &other)
    {
        if (handle != VK_NULL_HANDLE)
        {
            EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = handle]()
                {
                    vkDestroyFence(g_renderInterface->GetDevice()->GetDevice(), handle, nullptr);
                }));
        }

        handle = other.handle;
        lastFrameResult = other.lastFrameResult;
        isSubmitted = other.isSubmitted;

        other.handle = VK_NULL_HANDLE;
        other.lastFrameResult = VK_SUCCESS;
        other.isSubmitted = false;
    }
    
    return *this;
}

VulkanFence::~VulkanFence()
{
    if (handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = handle]()
            {
                vkDestroyFence(g_renderInterface->GetDevice()->GetDevice(), handle, nullptr);
            }));

        handle = VK_NULL_HANDLE;
    }
}

void VulkanFence::Create(bool createSignaled)
{
    Assert(handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    if (createSignaled)
    {
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VkResult result = vkCreateFence(g_renderInterface->GetDevice()->GetDevice(), &fenceCreateInfo, nullptr, &handle);
    Assert(result == VK_SUCCESS, "Failed to create Vulkan fence, VkResult: {}", result);
}

bool VulkanFence::CheckStatus()
{
    Assert(handle != VK_NULL_HANDLE);

    if (!isSubmitted)
    {
        return false;
    }

    VkResult result = vkGetFenceStatus(g_renderInterface->GetDevice()->GetDevice(), handle);

    if (result == VK_NOT_READY)
    {
        return false;
    }

    Assert(result == VK_SUCCESS);

    if (result == VK_SUCCESS)
    {
        return true;
    }

    return false;
}

void VulkanFence::Wait(bool timeoutLoop)
{
    Assert(handle != VK_NULL_HANDLE);

    VkResult result = VK_SUCCESS;

    do
    {
        result = vkWaitForFences(g_renderInterface->GetDevice()->GetDevice(), 1, &handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    while (result == VK_TIMEOUT && timeoutLoop);

    lastFrameResult = result;
    isSubmitted = false;

    Assert(result == VK_SUCCESS, "Failed to wait for Vulkan fence, VkResult: {}", result);
}

void VulkanFence::Reset()
{
    VkResult result = vkResetFences(g_renderInterface->GetDevice()->GetDevice(), 1, &handle);
    Assert(result == VK_SUCCESS, "Failed to reset Vulkan fence, VkResult: {}", result);

    isSubmitted = false;
}

} // namespace Hyperion
