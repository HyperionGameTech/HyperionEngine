/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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
