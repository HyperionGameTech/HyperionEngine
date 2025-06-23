/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanFence : public RenderObject<VulkanFence>
{
public:
    HYP_API VulkanFence();
    HYP_API virtual ~VulkanFence() override;

    HYP_FORCE_INLINE VkFence GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkResult GetLastFrameResult() const
    {
        return m_last_frame_result;
    }

    HYP_API RendererResult Create();
    HYP_API RendererResult Destroy();
    HYP_API RendererResult WaitForGPU(bool timeout_loop = false);
    HYP_API RendererResult Reset();

private:
    VkFence m_handle = VK_NULL_HANDLE;
    VkResult m_last_frame_result = VK_SUCCESS;
};

using VulkanFenceRef = RenderObjectHandle_Strong<VulkanFence>;
using VulkanFenceWeakRef = RenderObjectHandle_Weak<VulkanFence>;

} // namespace hyperion

#endif
