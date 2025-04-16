/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP

#include <core/containers/Array.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#define HYP_ENABLE_VSYNC 0

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <>
struct SwapchainPlatformImpl<Platform::VULKAN>
{
    Swapchain<Platform::VULKAN> *self = nullptr;

    VkSwapchainKHR              handle = VK_NULL_HANDLE;
    VkSurfaceKHR                surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR          surface_format;
    VkPresentModeKHR            present_mode;
    SwapchainSupportDetails     support_details;

    RendererResult Create();
    RendererResult Destroy();

    void ChooseSurfaceFormat();
    void ChoosePresentMode();
    void RetrieveSupportDetails();
    void RetrieveImageHandles();
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP

