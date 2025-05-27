/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FRAME_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FRAME_HPP

#include <rendering/backend/RendererFrame.hpp>

#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererSemaphore.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {
namespace renderer {

class VulkanFence;
using VulkanFenceRef = RenderObjectHandle_Strong<VulkanFence>;

class VulkanFrame final : public FrameBase
{
public:
    explicit VulkanFrame();
    explicit VulkanFrame(uint32 frame_index);
    virtual ~VulkanFrame() override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_FORCE_INLINE const VulkanFenceRef& GetFence() const
    {
        return m_queue_submit_fence;
    }

    HYP_FORCE_INLINE VulkanSemaphoreChain& GetPresentSemaphores()
    {
        return m_present_semaphores;
    }

    HYP_FORCE_INLINE const VulkanSemaphoreChain& GetPresentSemaphores() const
    {
        return m_present_semaphores;
    }

    RendererResult RecreateFence();

private:
    VulkanSemaphoreChain m_present_semaphores;
    VulkanFenceRef m_queue_submit_fence;
};

} // namespace renderer
} // namespace hyperion

#endif
