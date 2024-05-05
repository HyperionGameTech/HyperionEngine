/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/system/Debug.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

Swapchain<Platform::VULKAN>::Swapchain()
    : swapchain(nullptr),
      image_format(InternalFormat::NONE)
{
}

VkSurfaceFormatKHR Swapchain<Platform::VULKAN>::ChooseSurfaceFormat(Device<Platform::VULKAN> *device)
{
    DebugLog(LogType::Info, "Looking for SRGB surface format\n");

    /* look for srgb format */
    this->image_format = device->GetFeatures().FindSupportedSurfaceFormat(
        this->support_details,
        std::array {
            InternalFormat::RGBA8_SRGB,
            InternalFormat::BGRA8_SRGB
        },
        [this](const VkSurfaceFormatKHR &format)
        {
            if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return false;
            }

            this->surface_format = format;

            return true;
        }
    );

    if (this->image_format != InternalFormat::NONE) {
        return this->surface_format;
    }

    DebugLog(LogType::Warn, "Could not find SRGB surface format, looking for non-srgb format\n");

    /* look for non-srgb format */
    this->image_format = device->GetFeatures().FindSupportedSurfaceFormat(
        this->support_details,
        std::array{
            InternalFormat::R11G11B10F,
            InternalFormat::RGBA16F,
            InternalFormat::RGBA8
        },
        [this](const VkSurfaceFormatKHR &format)
        {
            this->surface_format = format;

            return true;
        }
    );

    AssertThrowMsg(this->image_format != InternalFormat::NONE, "Failed to find a surface format!");

    return this->surface_format;
}

VkPresentModeKHR Swapchain<Platform::VULKAN>::GetPresentMode()
{
    return HYP_ENABLE_VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
}

void Swapchain<Platform::VULKAN>::RetrieveSupportDetails(Device<Platform::VULKAN> *device)
{
    this->support_details = device->GetFeatures().QuerySwapchainSupport(device->GetRenderSurface());
}

void Swapchain<Platform::VULKAN>::RetrieveImageHandles(Device<Platform::VULKAN> *device)
{
    Array<VkImage> vk_images;
    uint32 image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(device->GetDevice(), this->swapchain, &image_count, nullptr);
    DebugLog(LogType::Warn, "image count %d\n", image_count);

    vk_images.Resize(image_count);

    vkGetSwapchainImagesKHR(device->GetDevice(), this->swapchain, &image_count, vk_images.Data());
    DebugLog(LogType::Info, "Retrieved Swapchain images\n");

    m_images.Resize(image_count);

    for (uint32 i = 0; i < image_count; i++) {
        m_images[i] = MakeRenderObject<Image<Platform::VULKAN>>(
            Extent3D { extent.width, extent.height, 1 },
            image_format,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            nullptr,
            IMAGE_FLAGS_NONE
        );

        m_images[i]->GetPlatformImpl().handle = vk_images[i];
        m_images[i]->GetPlatformImpl().is_handle_owned = false;

        HYPERION_ASSERT_RESULT(m_images[i]->Create(device));
    }
}

Result Swapchain<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, const VkSurfaceKHR &surface)
{
    RetrieveSupportDetails(device);

    this->surface_format = ChooseSurfaceFormat(device);
    this->present_mode = GetPresentMode();
    this->extent = {
        support_details.capabilities.currentExtent.width,
        support_details.capabilities.currentExtent.height
    };

    uint32 image_count = support_details.capabilities.minImageCount + 1;

    if (support_details.capabilities.maxImageCount > 0 && image_count > support_details.capabilities.maxImageCount) {
        image_count = support_details.capabilities.maxImageCount;
    }

    DebugLog(LogType::Debug, "Swapchain image count: %d\n", image_count);

    VkSwapchainCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    create_info.surface             = surface;
    create_info.minImageCount       = image_count;
    create_info.imageFormat         = surface_format.format;
    create_info.imageColorSpace     = surface_format.colorSpace;
    create_info.imageExtent         = {extent.width, extent.height};
    create_info.imageArrayLayers    = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage          = image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32 concurrent_families[] = {
        qf_indices.graphics_family.Get(),
        qf_indices.present_family.Get()
    };

    if (qf_indices.graphics_family.Get() != qf_indices.present_family.Get()) {
        DebugLog(LogType::Debug, "Swapchain sharing mode set to Concurrent\n");
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = uint32(std::size(concurrent_families)); /* Two family indices(one for each process) */
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

Result Swapchain<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_images));

    DebugLog(LogType::Debug, "Destroying swapchain\n");

    vkDestroySwapchainKHR(device->GetDevice(), this->swapchain, nullptr);

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion