/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>

#include <core/containers/HashSet.hpp>

namespace hyperion {

struct VulkanDeviceQueue;

HYP_CLASS(NoScriptBindings)
class VulkanFrame final : public FrameBase
{
    HYP_OBJECT_BODY(VulkanFrame);

public:
    explicit VulkanFrame();
    explicit VulkanFrame(uint32 frameIndex);
    virtual ~VulkanFrame() override;

    virtual RendererResult Create() override;
    virtual RendererResult ResetFrameState() override;

    HYP_FORCE_INLINE void AddRenderPass(VulkanRenderPass* renderPass)
    {
        HYP_GFX_ASSERT(renderPass != nullptr);
        m_renderPasses.Add(renderPass);
    }

    RendererResult Submit(VulkanDeviceQueue* deviceQueue, VulkanCommandBuffer* commandBuffer);

    HYP_FORCE_INLINE const VulkanFenceRef& GetFence() const
    {
        return m_queueSubmitFence;
    }

    HYP_FORCE_INLINE VulkanSemaphoreChain& GetPresentSemaphores()
    {
        return m_presentSemaphores;
    }

    HYP_FORCE_INLINE const VulkanSemaphoreChain& GetPresentSemaphores() const
    {
        return m_presentSemaphores;
    }

    RendererResult RecreateFence();

private:
    using VulkanRenderPassesSet = HashSet<
        VulkanRenderPass*,
        &KeyBy_Identity<VulkanRenderPass*>,
        NodeAllocator<VulkanAllocator>>;

    void UpdateRenderPasses();

    VulkanSemaphoreChain m_presentSemaphores;
    VulkanFenceRef m_queueSubmitFence;

    VulkanRenderPassesSet m_renderPasses;
};

} // namespace hyperion
