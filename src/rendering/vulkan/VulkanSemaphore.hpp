/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanSemaphore final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanSemaphore);

public:
    VulkanSemaphore();
    ~VulkanSemaphore() override;

    HYP_FORCE_INLINE VkSemaphore GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE bool IsCreated() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

    RendererResult Create();

private:
    VkSemaphore m_handle;
};

} // namespace Hyperion
