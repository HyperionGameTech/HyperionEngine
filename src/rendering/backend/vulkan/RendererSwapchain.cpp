/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererFrameHandler.hpp>

#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {
namespace platform {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

static const bool use_srgb = true;
static const VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

static RendererResult HandleNextFrame(
    Swapchain<Platform::VULKAN> *swapchain,
    Frame<Platform::VULKAN> *frame,
    uint32 *index,
    bool &out_needs_recreate
)
{
    VkResult vk_result = vkAcquireNextImageKHR(
        GetRenderingAPI()->GetDevice()->GetDevice(),
        swapchain->GetPlatformImpl().handle,
        UINT64_MAX,
        frame->GetPresentSemaphores().GetWaitSemaphores()[0].Get().GetSemaphore(),
        VK_NULL_HANDLE,
        index
    );

    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        out_needs_recreate = true;

        return { };
    } else if (vk_result != VK_SUCCESS && vk_result != VK_SUBOPTIMAL_KHR) {
        return HYP_MAKE_ERROR(RendererError, "Failed to acquire next image", int(vk_result));
    }

    return { };
}

#pragma region SwapchainPlatformImpl

RendererResult SwapchainPlatformImpl<Platform::VULKAN>::Create()
{
    if (surface == VK_NULL_HANDLE) {
        return HYP_MAKE_ERROR(RendererError, "Cannot initialize swapchain without a surface");
    }

    RetrieveSupportDetails();
    ChooseSurfaceFormat();
    ChoosePresentMode();

    self->m_extent = {
        support_details.capabilities.currentExtent.width,
        support_details.capabilities.currentExtent.height
    };

    if (self->m_extent.x * self->m_extent.y == 0) {
        return HYP_MAKE_ERROR(RendererError, "Failed to retrieve swapchain resolution!");
    }

    uint32 image_count = support_details.capabilities.minImageCount + 1;

    if (support_details.capabilities.maxImageCount > 0 && image_count > support_details.capabilities.maxImageCount) {
        image_count = support_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    create_info.surface             = surface;
    create_info.minImageCount       = image_count;
    create_info.imageFormat         = surface_format.format;
    create_info.imageColorSpace     = surface_format.colorSpace;
    create_info.imageExtent         = { self->m_extent.x, self->m_extent.y };
    create_info.imageArrayLayers    = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage          = image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    const QueueFamilyIndices &qf_indices = GetRenderingAPI()->GetDevice()->GetQueueFamilyIndices();

    const uint32 concurrent_families[] = {
        qf_indices.graphics_family.Get(),
        qf_indices.present_family.Get()
    };

    if (qf_indices.graphics_family.Get() != qf_indices.present_family.Get()) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = uint32(std::size(concurrent_families)); /* Two family indices(one for each process) */
        create_info.pQueueFamilyIndices = concurrent_families;
    } else {
        /* Computations and presentation are done on same hardware(most scenarios) */
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
        vkCreateSwapchainKHR(GetRenderingAPI()->GetDevice()->GetDevice(), &create_info, nullptr, &handle),
        "Failed to create Vulkan swapchain!"
    );

    RetrieveImageHandles();

    self->m_frame_handler = MakeRenderObject<FrameHandler<Platform::VULKAN>>(self->m_images.Size(), HandleNextFrame);
    HYPERION_BUBBLE_ERRORS(self->m_frame_handler->Create(&GetRenderingAPI()->GetDevice()->GetGraphicsQueue()));

    HYPERION_RETURN_OK;
}

RendererResult SwapchainPlatformImpl<Platform::VULKAN>::Destroy()
{
    self->m_frame_handler->Destroy();
    self->m_frame_handler.Reset();

    vkDestroySwapchainKHR(GetRenderingAPI()->GetDevice()->GetDevice(), handle, nullptr);
    handle = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

void SwapchainPlatformImpl<Platform::VULKAN>::ChooseSurfaceFormat()
{
    surface_format = { };

    if (use_srgb) {
        /* look for srgb format */
        self->m_image_format = GetRenderingAPI()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
            support_details,
            { { InternalFormat::RGBA8_SRGB, InternalFormat::BGRA8_SRGB } },
            [this](auto &&format)
            {
                if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return false;
                }

                surface_format = format;

                return true;
            }
        );

        if (self->m_image_format != InternalFormat::NONE) {
            return;
        }
    }

    /* look for non-srgb format */
    self->m_image_format = GetRenderingAPI()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
        support_details,
        { { InternalFormat::R11G11B10F, InternalFormat::RGBA16F, InternalFormat::RGBA8 } },
        [this](auto &&format)
        {
            surface_format = format;

            return true;
        }
    );

    AssertThrowMsg(self->m_image_format != InternalFormat::NONE, "Failed to find a surface format!");
}

void SwapchainPlatformImpl<Platform::VULKAN>::ChoosePresentMode()
{
    present_mode = HYP_ENABLE_VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
}

void SwapchainPlatformImpl<Platform::VULKAN>::RetrieveSupportDetails()
{
    support_details = GetRenderingAPI()->GetDevice()->GetFeatures().QuerySwapchainSupport(GetRenderingAPI()->GetDevice()->GetRenderSurface());
}

void SwapchainPlatformImpl<Platform::VULKAN>::RetrieveImageHandles()
{
    Array<VkImage> vk_images;
    uint32 image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(GetRenderingAPI()->GetDevice()->GetDevice(), handle, &image_count, nullptr);

    vk_images.Resize(image_count);

    vkGetSwapchainImagesKHR(GetRenderingAPI()->GetDevice()->GetDevice(), handle, &image_count, vk_images.Data());

    self->m_images.Resize(image_count);

    for (uint32 i = 0; i < image_count; i++) {
        self->m_images[i] = MakeRenderObject<Image<Platform::VULKAN>>(
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                self->m_image_format,
                Vec3u { self->m_extent.x, self->m_extent.y, 1 }
            }
        );

        self->m_images[i]->GetPlatformImpl().handle = vk_images[i];
        self->m_images[i]->GetPlatformImpl().is_handle_owned = false;

        HYPERION_ASSERT_RESULT(self->m_images[i]->Create());
    }
}

#pragma endregion SwapchainPlatformImpl

#pragma region Swapchain

template <>
Swapchain<Platform::VULKAN>::Swapchain()
    : m_platform_impl { this },
      m_image_format(InternalFormat::NONE)
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
RendererResult Swapchain<Platform::VULKAN>::Create()
{
    return m_platform_impl.Create();
}

template <>
RendererResult Swapchain<Platform::VULKAN>::Destroy()
{
    SafeRelease(std::move(m_images));
    
    return m_platform_impl.Destroy();
}

#pragma endregion Swapchain

} // namespace platform
} // namespace renderer
} // namespace hyperion