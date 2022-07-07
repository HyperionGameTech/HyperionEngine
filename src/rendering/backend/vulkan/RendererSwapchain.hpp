#ifndef HYPERION_RENDERER_SWAPCHAIN_H
#define HYPERION_RENDERER_SWAPCHAIN_H

#include "RendererStructs.hpp"
#include "RendererDevice.hpp"
#include "RendererImageView.hpp"
#include "RendererSemaphore.hpp"
#include "RendererFramebuffer.hpp"

#include <vector>

#define HYP_ENABLE_VSYNC 0

namespace hyperion {
namespace renderer {
class Swapchain {
    static constexpr VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkSurfaceFormatKHR ChooseSurfaceFormat(Device *device);
    VkPresentModeKHR GetPresentMode();
    void RetrieveSupportDetails(Device *device);
    void RetrieveImageHandles(Device *device);

public:
    static constexpr uint32_t max_frames_in_flight = 2;

    Swapchain();
    ~Swapchain() = default;

    Result Create(Device *device, const VkSurfaceKHR &surface);
    Result Destroy(Device *device);

    inline size_t NumImages() const { return images.size(); }

    VkSwapchainKHR          swapchain;
    Extent2D                extent;
    VkSurfaceFormatKHR      surface_format;
    Image::InternalFormat   image_format;
    std::vector<VkImage>    images;

private:
    SwapchainSupportDetails support_details;
    VkPresentModeKHR        present_mode;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_SWAPCHAIN_H

