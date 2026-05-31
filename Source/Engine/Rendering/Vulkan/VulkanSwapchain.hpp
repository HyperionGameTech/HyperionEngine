/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/Swapchain.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Vulkan/VulkanSemaphore.hpp>
#include <Rendering/Vulkan/VulkanFramebuffer.hpp>
#include <Rendering/Vulkan/VulkanStructs.hpp>

#include <Rendering/Shared.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

namespace Hyperion {

struct VulkanDeviceQueue;

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanSwapchain final : public SwapchainBase
{
    HYP_OBJECT_BODY(VulkanSwapchain);

public:
    VulkanSwapchain(VkSurfaceKHR surface, const Vec2u& extent);
    ~VulkanSwapchain() override;

    HYP_FORCE_INLINE VkSwapchainKHR GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkSurfaceKHR GetVulkanSurface() const
    {
        return m_surface;
    }

    HYP_FORCE_INLINE const Array<VulkanSemaphoreRef, VulkanAllocator>& GetPresentSemaphores() const
    {
        return m_presentSemaphores;
    }

    HYP_FORCE_INLINE const VulkanSemaphoreRef& GetCurrentPresentSemaphore() const
    {
        return m_presentSemaphores[m_acquiredImageIndex];
    }

    bool IsCreated() const override;

    void PrepareForFrame(VulkanFrame* frame);
    void PresentFrame(VulkanFrame* frame, VulkanDeviceQueue* queue);

    RendererResult Create() override;
    void SetExtent(Vec2u newExtent) override;
    void Recreate() override;

private:
    RendererResult ChooseSurfaceFormat();
    RendererResult RetrieveImageHandles();

    VkSwapchainKHR m_handle;
    VkSwapchainKHR m_oldHandle;
    VkSurfaceKHR m_surface;
    VkSurfaceFormatKHR m_surfaceFormat;
    VkPresentModeKHR m_presentMode;
    VulkanSwapchainSupportDetails m_supportDetails;
    Array<VulkanSemaphoreRef, VulkanAllocator> m_presentSemaphores;
};

} // namespace Hyperion
