#include "RendererSwapchain.hpp"
#include "RendererDevice.hpp"
#include "RendererFeatures.hpp"
#include "RendererImage.hpp"

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {
Swapchain::Swapchain()
    : swapchain(nullptr),
      image_format(Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE)
{
}

VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(Device *device)
{
    DebugLog(LogType::Debug, "Looking for SRGB surface format\n");

    /* look for srgb format */
    this->image_format = device->GetFeatures().FindSupportedSurfaceFormat(
        this->support_details,
        std::array{
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB
        },
        [this](const VkSurfaceFormatKHR &format) {
            if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return false;
            }

            this->surface_format = format;

            return true;
        }
    );

    if (this->image_format != Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE) {
        return this->surface_format;
    }

    DebugLog(LogType::Debug, "Could not find SRGB surface format, looking for non-srgb format\n");

    /* look for non-srgb format */
    this->image_format = device->GetFeatures().FindSupportedSurfaceFormat(
        this->support_details,
        std::array{
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F
        },
        [this](const VkSurfaceFormatKHR &format) {
            this->surface_format = format;

            return true;
        }
    );

    AssertThrowMsg(this->image_format != Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE, "Failed to find a surface format!");

    return this->surface_format;

    /*for (const auto &format : this->support_details.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    DebugLog(LogType::Warn, "Swapchain format sRGB is not supported, going with defaults...\n");
    return this->support_details.formats[0];*/
}

VkPresentModeKHR Swapchain::GetPresentMode()
{
    return HYP_ENABLE_VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
}

void Swapchain::RetrieveSupportDetails(Device *device)
{
    this->support_details = device->GetFeatures().QuerySwapchainSupport(device->GetRenderSurface());
}

void Swapchain::RetrieveImageHandles(Device *device)
{
    uint32_t image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(device->GetDevice(), this->swapchain, &image_count, nullptr);
    DebugLog(LogType::Warn, "image count %d\n", image_count);

    this->images.resize(image_count);
    vkGetSwapchainImagesKHR(device->GetDevice(), this->swapchain, &image_count, this->images.data());
    DebugLog(LogType::Info, "Retrieved Swapchain images\n");
}

Result Swapchain::Create(Device *device, const VkSurfaceKHR &surface)
{
    this->RetrieveSupportDetails(device);

    this->surface_format = this->ChooseSurfaceFormat(device);
    this->present_mode = this->GetPresentMode();
    this->extent = {
        support_details.capabilities.currentExtent.width,
        support_details.capabilities.currentExtent.height
    };

    uint32_t image_count = support_details.capabilities.minImageCount + 1;

    if (support_details.capabilities.maxImageCount > 0 && image_count > support_details.capabilities.maxImageCount) {
        image_count = support_details.capabilities.maxImageCount;
    }

    DebugLog(LogType::Debug, "Swapchain image count: %d\n", image_count);

    VkSwapchainCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    create_info.surface          = surface;
    create_info.minImageCount    = image_count;
    create_info.imageFormat      = surface_format.format;
    create_info.imageColorSpace  = surface_format.colorSpace;
    create_info.imageExtent    = {extent.width, extent.height};
    create_info.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage       = image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32_t concurrent_families[] = {
        qf_indices.graphics_family.value(),
        qf_indices.present_family.value()
    };

    if (qf_indices.graphics_family != qf_indices.present_family) {
        DebugLog(LogType::Debug, "Swapchain sharing mode set to Concurrent\n");
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = uint32_t(std::size(concurrent_families)); /* Two family indices(one for each process) */
        create_info.pQueueFamilyIndices = concurrent_families;
    } else {
        /* Computations and presentation are done on same hardware(most scenarios) */
        DebugLog(LogType::Debug, "Swapchain sharing mode set to Exclusive\n");
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;       /* Optional */
        create_info.pQueueFamilyIndices = nullptr; /* Optional */
    }

    /* For transformations such as rotations, etc. */
    create_info.preTransform = this->support_details.capabilities.currentTransform;
    /* This can be used to blend with other windows in the windowing system, but we
     * simply leave it opaque.*/
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode  = present_mode;
    create_info.clipped      = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    HYPERION_VK_CHECK_MSG(
        vkCreateSwapchainKHR(device->GetDevice(), &create_info, nullptr, &this->swapchain),
        "Failed to create Vulkan swapchain!"
    );

    DebugLog(LogType::Debug, "Created Swapchain!\n");

    this->RetrieveImageHandles(device);

    HYPERION_RETURN_OK;
}

Result Swapchain::Destroy(Device *device)
{
    DebugLog(LogType::Debug, "Destroying swapchain\n");

    vkDestroySwapchainKHR(device->GetDevice(), this->swapchain, nullptr);

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion