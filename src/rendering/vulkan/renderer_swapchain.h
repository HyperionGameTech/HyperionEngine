//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_SWAPCHAIN_H
#define HYPERION_RENDERER_SWAPCHAIN_H

#include "renderer_structs.h"

#include "renderer_device.h"

#include <vector>

namespace hyperion {

class RendererSwapchain {
    VkSurfaceFormatKHR ChooseSurfaceFormat();

    VkPresentModeKHR ChoosePresentMode();

    VkExtent2D ChooseSwapchainExtent();

    void RetrieveImageHandles();

    void CreateImageView(size_t index, VkImage *swapchain_image);

    void CreateImageViews();

    void DestroyImageViews();

public:
    RendererSwapchain(RendererDevice *_device, const SwapchainSupportDetails &_details);

    ~RendererSwapchain();

    void Create(const VkSurfaceKHR &surface, QueueFamilyIndices qf_indices);

    void CreateFramebuffers(VkRenderPass *renderpass);

    std::vector<VkFramebuffer> framebuffers;

    VkSwapchainKHR swapchain;
    VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;

    VkFormat image_format;
private:
    RendererDevice *renderer_device = nullptr;
    SwapchainSupportDetails support_details;

    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
};
}; /* namespace hyperion */


#endif //HYPERION_RENDERER_SWAPCHAIN_H

