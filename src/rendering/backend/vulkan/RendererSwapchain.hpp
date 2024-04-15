/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP

#include <core/lib/DynArray.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vector>

#define HYP_ENABLE_VSYNC 0

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <>
class Swapchain<Platform::VULKAN>
{
    static constexpr VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkSurfaceFormatKHR ChooseSurfaceFormat(Device<Platform::VULKAN> *device);
    VkPresentModeKHR GetPresentMode();
    void RetrieveSupportDetails(Device<Platform::VULKAN> *device);
    void RetrieveImageHandles(Device<Platform::VULKAN> *device);

public:
    Swapchain();
    ~Swapchain() = default;

    Result Create(Device<Platform::VULKAN> *device, const VkSurfaceKHR &surface);
    Result Destroy(Device<Platform::VULKAN> *device);

    SizeType NumImages() const
        { return m_images.Size(); }

    const Array<ImageRef<Platform::VULKAN>> &GetImages() const
        { return m_images; }

    VkSwapchainKHR                      swapchain;
    Extent2D                            extent;
    VkSurfaceFormatKHR                  surface_format;
    InternalFormat                      image_format;

private:
    Array<ImageRef<Platform::VULKAN>>   m_images;

    SwapchainSupportDetails             support_details;
    VkPresentModeKHR                    present_mode;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_SWAPCHAIN_HPP

