#ifndef HYPERION_RENDERER_SWAPCHAIN_H
#define HYPERION_RENDERER_SWAPCHAIN_H

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
class Swapchain
{
    static constexpr VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkSurfaceFormatKHR ChooseSurfaceFormat(Device *device);
    VkPresentModeKHR GetPresentMode();
    void RetrieveSupportDetails(Device *device);
    void RetrieveImageHandles(Device *device);

public:
    Swapchain();
    ~Swapchain() = default;

    Result Create(Device *device, const VkSurfaceKHR &surface);
    Result Destroy(Device *device);

    SizeType NumImages() const { return images.Size(); }

    VkSwapchainKHR swapchain;
    Extent2D extent;
    VkSurfaceFormatKHR surface_format;
    InternalFormat image_format;
    Array<PlatformImage> images;

private:
    SwapchainSupportDetails support_details;
    VkPresentModeKHR present_mode;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_SWAPCHAIN_H

