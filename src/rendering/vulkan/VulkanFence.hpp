/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanFence final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanFence);

public:
    VulkanFence();
    virtual ~VulkanFence() override;

    HYP_FORCE_INLINE VkFence GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkResult GetLastFrameResult() const
    {
        return m_lastFrameResult;
    }

    RendererResult Create();
    RendererResult Wait(bool timeoutLoop = false);
    RendererResult Reset();

private:
    VkFence m_handle = VK_NULL_HANDLE;
    VkResult m_lastFrameResult = VK_SUCCESS;
};

} // namespace hyperion
