/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
        return m_handle;
    }

    HYP_FORCE_INLINE VkResult GetLastFrameResult() const
    {
        return m_lastFrameResult;
    }

    void Create(bool createSignaled = false);
    void Wait(bool timeoutLoop = false);
    void Reset();

private:
    VkFence m_handle = VK_NULL_HANDLE;
    VkResult m_lastFrameResult = VK_SUCCESS;
};

} // namespace Hyperion
