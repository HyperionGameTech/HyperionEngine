/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/system/Debug.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

static const bool use_srgb = true;
static const VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

#pragma region SwapchainPlatformImpl

Result SwapchainPlatformImpl<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    if (surface == VK_NULL_HANDLE) {
        return { Result::RENDERER_ERR, "Cannot initialize swapchain without a surface" };
    }

    RetrieveSupportDetails(device);
    ChooseSurfaceFormat(device);
    ChoosePresentMode();

    self->extent = {
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
    create_info.imageExtent         = { self->extent.x, self->extent.y };
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
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    HYPERION_VK_CHECK_MSG(
        vkCreateSwapchainKHR(device->GetDevice(), &create_info, nullptr, &handle),
        "Failed to create Vulkan swapchain!"
    );

    RetrieveImageHandles(device);

    HYPERION_RETURN_OK;
}

Result SwapchainPlatformImpl<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    DebugLog(LogType::Debug, "Destroying swapchain\n");

    vkDestroySwapchainKHR(device->GetDevice(), handle, nullptr);
    handle = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

void SwapchainPlatformImpl<Platform::VULKAN>::ChooseSurfaceFormat(Device<Platform::VULKAN> *device)
{
    DebugLog(LogType::Info, "Looking for SRGB surface format\n");

    surface_format = { };

    if (use_srgb) {
        /* look for srgb format */
        self->image_format = device->GetFeatures().FindSupportedSurfaceFormat(
            support_details,
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

        if (self->image_format != InternalFormat::NONE) {
            return;
        }

        DebugLog(LogType::Warn, "Could not find SRGB surface format, looking for non-srgb format\n");
    }

    /* look for non-srgb format */
    self->image_format = device->GetFeatures().FindSupportedSurfaceFormat(
        support_details,
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

    AssertThrowMsg(self->image_format != InternalFormat::NONE, "Failed to find a surface format!");
}

void SwapchainPlatformImpl<Platform::VULKAN>::ChoosePresentMode()
{
    present_mode = HYP_ENABLE_VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
}

void SwapchainPlatformImpl<Platform::VULKAN>::RetrieveSupportDetails(Device<Platform::VULKAN> *device)
{
    support_details = device->GetFeatures().QuerySwapchainSupport(device->GetRenderSurface());
}

void SwapchainPlatformImpl<Platform::VULKAN>::RetrieveImageHandles(Device<Platform::VULKAN> *device)
{
    Array<VkImage> vk_images;
    uint32 image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(device->GetDevice(), handle, &image_count, nullptr);
    DebugLog(LogType::Warn, "image count %d\n", image_count);

    vk_images.Resize(image_count);

    vkGetSwapchainImagesKHR(device->GetDevice(), handle, &image_count, vk_images.Data());
    DebugLog(LogType::Info, "Retrieved Swapchain images\n");

    self->m_images.Resize(image_count);

    for (uint32 i = 0; i < image_count; i++) {
        self->m_images[i] = MakeRenderObject<Image<Platform::VULKAN>>(
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                self->image_format,
                Vec3u { self->extent.x, self->extent.y, 1 }
            }
        );

        self->m_images[i]->GetPlatformImpl().handle = vk_images[i];
        self->m_images[i]->GetPlatformImpl().is_handle_owned = false;

        HYPERION_ASSERT_RESULT(self->m_images[i]->Create(device));
    }
}

#pragma endregion SwapchainPlatformImpl

#pragma region Swapchain

template <>
Swapchain<Platform::VULKAN>::Swapchain()
    : m_platform_impl { this },
      image_format(InternalFormat::NONE)
{
}

template <>
Swapchain<Platform::VULKAN>::~Swapchain()
{
    AssertThrow(m_platform_impl.handle == VK_NULL_HANDLE);
}

template <>
bool Swapchain<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.handle != VK_NULL_HANDLE;
}

template <>
Result Swapchain<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    return m_platform_impl.Create(device);
}

template <>
Result Swapchain<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_images));
    
    return m_platform_impl.Destroy(device);
}

#pragma endregion Swapchain

} // namespace platform
} // namespace renderer
} // namespace hyperion