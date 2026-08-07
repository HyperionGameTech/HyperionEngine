/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanFence.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanResult.hpp>

#include <Rendering/CrashHandler.hpp>

#include <Rendering/Device.hpp>

#define DEFAULT_FENCE_TIMEOUT 100000000000

#include <VulkanFence.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

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
            EnqueueDeletion(FunctionWrapper<Proc<void()>>(
                [handle = handle]()
                {
                    vkDestroyFence(RI.GetDevice()->GetDevice(), handle, nullptr);
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
                                                          vkDestroyFence(RI.GetDevice()->GetDevice(), handle, nullptr);
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

    VkResult result = vkCreateFence(RI.GetDevice()->GetDevice(), &fenceCreateInfo, nullptr, &handle);
    Assert(result == VK_SUCCESS, "Failed to create Vulkan fence, VkResult: {}", result);
#ifdef HYP_RHI_DEBUG_NAMES
    SetDebugName(debugName);
#endif
}

bool VulkanFence::CheckStatus()
{
    if (HYP_UNLIKELY(handle == VK_NULL_HANDLE))
    {
        return false;
    }

    if (!isSubmitted)
    {
        return false;
    }

    VkResult result = vkGetFenceStatus(RI.GetDevice()->GetDevice(), handle);

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
        result = vkWaitForFences(RI.GetDevice()->GetDevice(), 1, &handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    while (result == VK_TIMEOUT && timeoutLoop);

    lastFrameResult = result;
    isSubmitted = false;

    if (HYP_UNLIKELY(result != VK_SUCCESS))
    {
        CrashHandler::Dump();
        return;

        HYP_FAIL("Failed to wait for Vulkan fence, VkResult: {}", result);
    }
}

void VulkanFence::Reset()
{
    VkResult result = vkResetFences(RI.GetDevice()->GetDevice(), 1, &handle);
    Assert(result == VK_SUCCESS, "Failed to reset Vulkan fence, VkResult: {}", result);

    isSubmitted = false;
}

#ifdef HYP_RHI_DEBUG_NAMES
void VulkanFence::SetDebugName(Name name)
{
    debugName = name;

    if (handle == VK_NULL_HANDLE)
    {
        return;
    }

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_FENCE;
        objectNameInfo.objectHandle = (uint64)handle;
        objectNameInfo.pObjectName = name.LookupString();

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}
#endif

} // namespace Hyperion
