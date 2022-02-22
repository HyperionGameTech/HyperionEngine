//
// Created by emd22 on 2022-02-20.
//

#include "renderer_swapchain.h"

#include "../../system/debug.h"

namespace hyperion {

RendererSwapchain::RendererSwapchain(RendererDevice *_device, const SwapchainSupportDetails &_details) {
    this->swapchain = nullptr;
    this->renderer_device = _device;
    this->support_details = _details;
}

VkSurfaceFormatKHR RendererSwapchain::ChooseSurfaceFormat() {
    for (const auto &format: this->support_details.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    DebugLog(LogType::Warn, "Swapchain format sRGB is not supported, going with defaults...\n");
    return this->support_details.formats[0];
}

VkPresentModeKHR RendererSwapchain::ChoosePresentMode() {
    /* TODO: add more presentation modes in the future */
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RendererSwapchain::ChooseSwapchainExtent() {
    return this->support_details.capabilities.currentExtent;
}

void RendererSwapchain::RetrieveImageHandles() {
    uint32_t image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(this->renderer_device->GetDevice(), this->swapchain, &image_count, nullptr);
    this->images.resize(image_count);
    vkGetSwapchainImagesKHR(this->renderer_device->GetDevice(), this->swapchain, &image_count, this->images.data());
    DebugLog(LogType::Info, "Retrieved Swapchain images\n");
}

void RendererSwapchain::CreateImageView(size_t index, VkImage *swapchain_image) {
    VkImageViewCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    create_info.image = (*swapchain_image);
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = this->image_format;
    /* Components; we'll just stick to the default mapping for now. */
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(this->renderer_device->GetDevice(), &create_info, nullptr, &this->image_views[index]) !=
        VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create swapchain image views!\n");
        throw std::runtime_error("Could not create image views");
    }
}

void RendererSwapchain::CreateImageViews() {
    this->image_views.resize(this->images.size());
    for (size_t i = 0; i < this->images.size(); i++) {
        this->CreateImageView(i, &this->images[i]);
    }
}

void RendererSwapchain::DestroyImageViews() {
    for (auto view: this->image_views) {
        vkDestroyImageView(this->renderer_device->GetDevice(), view, nullptr);
    }
}

void RendererSwapchain::CreateFramebuffers(VkRenderPass *renderpass) {
    this->framebuffers.resize(this->image_views.size());
    DebugLog(LogType::Info, "[%d] image views found!\n", this->image_views.size());
    for (size_t i = 0; i < this->image_views.size(); i++) {
        VkImageView attachments[] = {this->image_views[i]};
        VkFramebufferCreateInfo create_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        create_info.renderPass = *renderpass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = attachments;
        create_info.width = this->extent.width;
        create_info.height = this->extent.height;
        create_info.layers = 1;

        if (vkCreateFramebuffer(this->renderer_device->GetDevice(), &create_info, nullptr, &this->framebuffers[i]) !=
            VK_SUCCESS) {
            DebugLog(LogType::Error, "Could not create Vulkan framebuffer!\n");
            throw std::runtime_error("could not create framebuffer");
        }
        DebugLog(LogType::Info, "Created Pipeline Framebuffer #%d\n", i);
    }
}

void RendererSwapchain::Create(const VkSurfaceKHR &surface, QueueFamilyIndices qf_indices) {
    this->surface_format = this->ChooseSurfaceFormat();
    this->present_mode = this->ChoosePresentMode();
    this->extent = this->ChooseSwapchainExtent();
    this->image_format = this->surface_format.format;

    uint32_t image_count = this->support_details.capabilities.minImageCount + 1;

    VkSwapchainCreateInfoKHR create_info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage = this->image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    if (qf_indices.graphics_family != qf_indices.present_family) {
        DebugLog(LogType::Info, "Swapchain sharing mode set to Concurrent!\n");
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2; /* Two family indices(one for each process) */
        const uint32_t families[] = {qf_indices.graphics_family.value(), qf_indices.present_family.value()};
        create_info.pQueueFamilyIndices = families;
    }
        /* Computations and presentation are done on same hardware(most scenarios) */
    else {
        DebugLog(LogType::Info, "Swapchain sharing mode set to Exclusive!\n");
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;       /* Optional */
        create_info.pQueueFamilyIndices = nullptr; /* Optional */
    }
    /* For transformations such as rotations, etc. */
    create_info.preTransform = this->support_details.capabilities.currentTransform;
    /* This can be used to blend with other windows in the windowing system, but we
     * simply leave it opaque.*/
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(this->renderer_device->GetDevice(), &create_info, nullptr, &this->swapchain) !=
        VK_SUCCESS) {
        DebugLog(LogType::Error, "Failed to create Vulkan swapchain!\n");
        throw std::runtime_error("Failed to create swapchain");
    }
    DebugLog(LogType::Info, "Created Swapchain!\n");
    this->RetrieveImageHandles();
    this->CreateImageViews();
}

RendererSwapchain::~RendererSwapchain() {
    for (auto framebuffer: this->framebuffers) {
        vkDestroyFramebuffer(this->renderer_device->GetDevice(), framebuffer, nullptr);
    }
    this->DestroyImageViews();
    vkDestroySwapchainKHR(this->renderer_device->GetDevice(), this->swapchain, nullptr);
    DebugLog(LogType::Info, "Destroying swapchain\n");
}

};