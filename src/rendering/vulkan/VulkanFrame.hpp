/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderFrame.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>

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

    virtual bool IsCreated() const override
    {
        return m_queueSubmitFence.IsValid();
    }

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

    HYP_FORCE_INLINE const VulkanSemaphoreRef& GetImageAvailableSemaphore() const
    {
        return m_imageAvailableSemaphore;
    }

    HYP_FORCE_INLINE const VulkanSemaphoreRef& GetRenderFinishedSemaphore() const
    {
        return m_renderFinishedSemaphore;
    }

    void RecreateFence();
    void RecreateSemaphores();

    void ResetRenderPassStates();

private:
    using VulkanRenderPassesSet = HashSet<
        VulkanRenderPass*,
        &KeyBy_Identity<VulkanRenderPass*>,
        NodeAllocator<VulkanAllocator>>;

    VulkanSemaphoreRef m_imageAvailableSemaphore;
    VulkanSemaphoreRef m_renderFinishedSemaphore;
    VulkanFenceRef m_queueSubmitFence;
    VulkanRenderPassesSet m_renderPasses;
};

} // namespace hyperion
