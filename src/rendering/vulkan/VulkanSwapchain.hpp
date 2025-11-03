/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <core/containers/Array.hpp>

#include <rendering/RenderSwapchain.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/Shared.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#define HYP_ENABLE_VSYNC 0

namespace hyperion {

struct VulkanDeviceQueue;

HYP_CLASS(NoScriptBindings)
class VulkanSwapchain final : public SwapchainBase
{
    HYP_OBJECT_BODY(VulkanSwapchain);

public:
    friend class VulkanInstance;

    VulkanSwapchain();
    virtual ~VulkanSwapchain() override;

    HYP_FORCE_INLINE VkSwapchainKHR GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE const VulkanFrameRef& GetCurrentFrame() const
    {
        return m_frames[m_currentFrameIndex];
    }

    HYP_FORCE_INLINE const VulkanCommandBufferRef& GetCurrentCommandBuffer() const
    {
        return m_commandBuffers[m_currentFrameIndex];
    }

    HYP_FORCE_INLINE uint32 NumAcquiredImages() const
    {
        return uint32(m_images.Size());
    }

    virtual bool IsCreated() const override;

    void NextFrame();

    RendererResult PrepareFrame(bool& outNeedsRecreate);
    RendererResult PresentFrame(VulkanDeviceQueue* queue) const;

    virtual RendererResult Create() override;

private:
    RendererResult ChooseSurfaceFormat();
    RendererResult ChoosePresentMode();
    RendererResult RetrieveSupportDetails();
    RendererResult RetrieveImageHandles();

    FixedArray<VulkanFrameRef, NumFramesInFlight> m_frames;
    FixedArray<VulkanCommandBufferRef, NumFramesInFlight> m_commandBuffers;

    VkSwapchainKHR m_handle;
    VkSurfaceKHR m_surface;
    VkSurfaceFormatKHR m_surfaceFormat;
    VkPresentModeKHR m_presentMode;
    VulkanSwapchainSupportDetails m_supportDetails;
};

} // namespace hyperion
