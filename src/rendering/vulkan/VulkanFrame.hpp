/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Frame.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>

#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>

#include <core/containers/HashSet.hpp>

namespace Hyperion {

struct VulkanDeviceQueue;

HYP_CLASS(NoScriptBindings)
class VulkanFrame final : public FrameBase
{
    HYP_OBJECT_BODY(VulkanFrame);

public:
    VulkanFrame();
    explicit VulkanFrame(uint32 frameIndex);
    ~VulkanFrame() override;

    bool IsCreated() const override
    {
        return m_queueSubmitFence.IsValid();
    }

    RendererResult Create() override;
    RendererResult ResetFrameState() override;

    HYP_FORCE_INLINE void AddRenderPass(VulkanRenderPass* renderPass)
    {
        HYP_GFX_ASSERT(renderPass != nullptr);
        m_renderPasses.Add(renderPass);
    }

    RendererResult Submit(
        VulkanDeviceQueue* deviceQueue,
        VulkanCommandBuffer* commandBuffer,
        VulkanSwapchain* swapchain = nullptr);

    HYP_FORCE_INLINE const VulkanFenceRef& GetFence() const
    {
        return m_queueSubmitFence;
    }

    VulkanSemaphore* GetImageAvailableSemaphore(const VulkanSwapchain* swapchain, bool createIfNotExist = true);

    void RecreateFence();
    void RecreateSemaphores(const VulkanSwapchain* swapchain);

    void ResetTransientStates();

private:
    using VulkanRenderPassesSet = HashSet<
        VulkanRenderPass*,
        &KeyBy_Identity<VulkanRenderPass*>,
        NodeAllocator<VulkanAllocator>>;

    struct VulkanSwapchainData
    {
        VulkanSwapchainWeakRef swapchainWeak; // always keep a weak ref so we can check validatity when iterating
        VulkanSemaphoreRef imageAvailableSemaphore;
    };

    static void InitVulkanSwapchainData(VulkanSwapchainData& swapchainData);

    VulkanFenceRef m_queueSubmitFence;
    VulkanRenderPassesSet m_renderPasses;

    HashMap<const VulkanSwapchain*, VulkanSwapchainData> m_swapchainData;
};

} // namespace Hyperion
