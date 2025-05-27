/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP

#include <core/containers/Array.hpp>

#include <rendering/backend/RendererSwapchain.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/vulkan/RendererSemaphore.hpp>
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#define HYP_ENABLE_VSYNC 0

namespace hyperion {
namespace renderer {

class VulkanSwapchain final : public SwapchainBase
{
public:
    friend class platform::Instance<Platform::vulkan>;

    HYP_API VulkanSwapchain();
    HYP_API virtual ~VulkanSwapchain() override;

    HYP_FORCE_INLINE VkSwapchainKHR GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE const VulkanFrameRef& GetCurrentFrame() const
    {
        return m_frames[m_current_frame_index];
    }

    HYP_FORCE_INLINE const VulkanCommandBufferRef& GetCurrentCommandBuffer() const
    {
        return m_command_buffers[m_current_frame_index];
    }

    HYP_FORCE_INLINE uint32 NumAcquiredImages() const
    {
        return uint32(m_images.Size());
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API void NextFrame();

    HYP_API RendererResult PrepareFrame(bool& out_needs_recreate);
    HYP_API RendererResult PresentFrame(VulkanDeviceQueue* queue) const;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

private:
    RendererResult ChooseSurfaceFormat();
    RendererResult ChoosePresentMode();
    RendererResult RetrieveSupportDetails();
    RendererResult RetrieveImageHandles();

    FixedArray<VulkanFrameRef, max_frames_in_flight> m_frames;
    FixedArray<VulkanCommandBufferRef, max_frames_in_flight> m_command_buffers;

    VkSwapchainKHR m_handle;
    VkSurfaceKHR m_surface;
    VkSurfaceFormatKHR m_surface_format;
    VkPresentModeKHR m_present_mode;
    SwapchainSupportDetails m_support_details;
};

} // namespace renderer
} // namespace hyperion

#endif // HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP
