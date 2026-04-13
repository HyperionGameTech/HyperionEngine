/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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

#include <Core/containers/HashSet.hpp>

namespace Hyperion {

struct VulkanDeviceQueue;

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanFrame final : public FrameBase
{
    HYP_OBJECT_BODY(VulkanFrame);

public:
    VulkanFrame();
    explicit VulkanFrame(uint32 frameIndex);

    ~VulkanFrame() override;

    HYP_FORCE_INLINE void AddRenderPass(VulkanRenderPass* renderPass)
    {
        Assert(renderPass != nullptr);
        m_renderPasses.Add(renderPass);
    }

    bool IsCreated() const override
    {
        return m_queueSubmitFence != nullptr;
    }

    RendererResult Create() override;

    void OnFrameStart() override;

    void WriteCommandBuffer(VulkanCommandBuffer* commandBuffer) override;

    HYP_FORCE_INLINE VulkanFence* GetFence() const
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
        VulkanAllocator,
        HashTablePolicy::NotPooled>;

    struct VulkanSwapchainData
    {
        VulkanSwapchainWeakRef swapchainWeak; // always keep a weak ref so we can check validatity when iterating
        VulkanSemaphore* imageAvailableSemaphore = nullptr;
    };

    static void InitVulkanSwapchainData(VulkanSwapchainData& swapchainData);

    VulkanFence* m_queueSubmitFence;
    VulkanRenderPassesSet m_renderPasses;

    HashMap<const VulkanSwapchain*, VulkanSwapchainData, VulkanAllocator> m_swapchainData;
};

} // namespace Hyperion
