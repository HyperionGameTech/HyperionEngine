/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanFence final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanFence);

public:
    static Pool* GetAllocator() { return g_vulkanPool; }

    VulkanFence();

    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;

    VulkanFence(VulkanFence&& other) noexcept
        : handle(other.handle),
          lastFrameResult(other.lastFrameResult),
          isSubmitted(other.isSubmitted)
    {
        other.handle = VK_NULL_HANDLE;
        other.lastFrameResult = VK_SUCCESS;
        other.isSubmitted = false;
    }

    VulkanFence& operator=(VulkanFence&& other) noexcept;

    ~VulkanFence() override;

    HYP_FORCE_INLINE VkFence GetVulkanHandle() const
    {
        return handle;
    }
    
    bool CheckStatus();

    void Create(bool createSignaled = false);
    void Wait(bool timeoutLoop = false);
    void Reset();
    
    VkFence handle;
    VkResult lastFrameResult;
    bool isSubmitted;
};

} // namespace Hyperion
