/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererSwapchain.hpp>
#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderingAPI* g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI* GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI*>(g_rendering_api);
}

static const bool use_srgb = true;
static const VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

static RendererResult HandleNextFrame(
    VulkanSwapchain* swapchain,
    VulkanFrame* frame,
    uint32* index,
    bool& out_needs_recreate)
{
    VkResult vk_result = vkAcquireNextImageKHR(
        GetRenderingAPI()->GetDevice()->GetDevice(),
        swapchain->GetVulkanHandle(),
        UINT64_MAX,
        frame->GetPresentSemaphores().GetWaitSemaphores()[0].Get().GetVulkanHandle(),
        VK_NULL_HANDLE,
        index);

    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        out_needs_recreate = true;

        return {};
    }
    else if (vk_result != VK_SUCCESS && vk_result != VK_SUBOPTIMAL_KHR)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to acquire next image", int(vk_result));
    }

    return {};
}

#pragma region Swapchain

VulkanSwapchain::VulkanSwapchain()
{
}

VulkanSwapchain::~VulkanSwapchain()
{
    AssertThrow(m_handle == VK_NULL_HANDLE);
}

bool VulkanSwapchain::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

void VulkanSwapchain::NextFrame()
{
    m_current_frame_index = (m_current_frame_index + 1) % max_frames_in_flight;
}

RendererResult VulkanSwapchain::PrepareFrame(bool& out_needs_recreate)
{
    static const auto handle_frame_result = [](VkResult result, bool& out_needs_recreate) -> RendererResult
    {
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            out_needs_recreate = true;

            HYPERION_RETURN_OK;
        }

        HYPERION_VK_CHECK(result);

        HYPERION_RETURN_OK;
    };

    const VulkanFrameRef& frame = GetCurrentFrame();

    HYPERION_BUBBLE_ERRORS(frame->GetFence()->WaitForGPU(true));

    HYPERION_BUBBLE_ERRORS(handle_frame_result(frame->GetFence()->GetLastFrameResult(), out_needs_recreate));

    HYPERION_BUBBLE_ERRORS(frame->ResetFrameState());

    HYPERION_BUBBLE_ERRORS(HandleNextFrame(this, frame, &m_acquired_image_index, out_needs_recreate));

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::PresentFrame(VulkanDeviceQueue* queue) const
{
    const VulkanFrameRef& frame = GetCurrentFrame();

    const auto& signal_semaphores = frame->GetPresentSemaphores().GetSignalSemaphoresView();

    VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = uint32(signal_semaphores.size());
    present_info.pWaitSemaphores = signal_semaphores.data();
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_handle;
    present_info.pImageIndices = &m_acquired_image_index;
    present_info.pResults = nullptr;

    HYPERION_VK_CHECK(vkQueuePresentKHR(queue->queue, &present_info));

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::Create()
{
    if (m_surface == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot initialize swapchain without a surface");
    }

    RetrieveSupportDetails();
    ChooseSurfaceFormat();
    ChoosePresentMode();

    m_extent = {
        m_support_details.capabilities.currentExtent.width,
        m_support_details.capabilities.currentExtent.height
    };

    if (m_extent.x * m_extent.y == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to retrieve swapchain resolution!");
    }

    uint32 image_count = m_support_details.capabilities.minImageCount + 1;

    if (m_support_details.capabilities.maxImageCount > 0 && image_count > m_support_details.capabilities.maxImageCount)
    {
        image_count = m_support_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = m_surface_format.format;
    create_info.imageColorSpace = m_surface_format.colorSpace;
    create_info.imageExtent = { m_extent.x, m_extent.y };
    create_info.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage = image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    const QueueFamilyIndices& qf_indices = GetRenderingAPI()->GetDevice()->GetQueueFamilyIndices();

    const uint32 concurrent_families[] = {
        qf_indices.graphics_family.Get(),
        qf_indices.present_family.Get()
    };

    if (qf_indices.graphics_family.Get() != qf_indices.present_family.Get())
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = uint32(std::size(concurrent_families)); /* Two family indices(one for each process) */
        create_info.pQueueFamilyIndices = concurrent_families;
    }
    else
    {
        /* Computations and presentation are done on same hardware(most scenarios) */
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;     /* Optional */
        create_info.pQueueFamilyIndices = nullptr; /* Optional */
    }

    /* For transformations such as rotations, etc. */
    create_info.preTransform = m_support_details.capabilities.currentTransform;
    /* This can be used to blend with other windows in the windowing system, but we
     * simply leave it opaque.*/
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = m_present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    HYPERION_VK_CHECK_MSG(
        vkCreateSwapchainKHR(GetRenderingAPI()->GetDevice()->GetDevice(), &create_info, nullptr, &m_handle),
        "Failed to create Vulkan swapchain!");

    RetrieveImageHandles();

    for (const ImageRef& image : m_images)
    {
        VulkanFramebufferRef framebuffer = MakeRenderObject<VulkanFramebuffer>(m_extent, RenderPassStage::PRESENT);

        framebuffer->AddAttachment(
            0,
            VulkanImageRef(image),
            LoadOperation::CLEAR,
            StoreOperation::STORE);

        HYPERION_BUBBLE_ERRORS(framebuffer->Create());

        m_framebuffers.PushBack(std::move(framebuffer));
    }

    VulkanDeviceQueue* queue = &GetRenderingAPI()->GetDevice()->GetGraphicsQueue();

    for (uint32 i = 0; i < m_frames.Size(); i++)
    {
        VkCommandPool pool = queue->command_pools[0];
        AssertThrow(pool != VK_NULL_HANDLE);

        VulkanCommandBufferRef command_buffer = MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
        HYPERION_BUBBLE_ERRORS(command_buffer->Create(pool));
        m_command_buffers[i] = std::move(command_buffer);

        VulkanFrameRef frame = MakeRenderObject<VulkanFrame>(i);
        HYPERION_BUBBLE_ERRORS(frame->Create());
        m_frames[i] = std::move(frame);
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::Destroy()
{
    if (m_handle == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Swapchain already destroyed");
    }

    SafeRelease(std::move(m_images));
    SafeRelease(std::move(m_framebuffers));
    SafeRelease(std::move(m_frames));
    SafeRelease(std::move(m_command_buffers));

    vkDestroySwapchainKHR(GetRenderingAPI()->GetDevice()->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::ChooseSurfaceFormat()
{
    m_surface_format = {};

    if (use_srgb)
    {
        /* look for srgb format */
        m_image_format = GetRenderingAPI()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
            m_support_details,
            { { InternalFormat::RGBA8_SRGB, InternalFormat::BGRA8_SRGB } },
            [this](auto&& format)
            {
                if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return false;
                }

                m_surface_format = format;

                return true;
            });

        if (m_image_format != InternalFormat::NONE)
        {
            HYPERION_RETURN_OK;
        }
    }

    /* look for non-srgb format */
    m_image_format = GetRenderingAPI()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
        m_support_details,
        { { InternalFormat::R11G11B10F, InternalFormat::RGBA16F, InternalFormat::RGBA8 } },
        [this](auto&& format)
        {
            m_surface_format = format;

            return true;
        });

    if (m_image_format == InternalFormat::NONE)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to find a supported surface format");
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::ChoosePresentMode()
{
    m_present_mode = HYP_ENABLE_VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::RetrieveSupportDetails()
{
    m_support_details = GetRenderingAPI()->GetDevice()->GetFeatures().QuerySwapchainSupport(GetRenderingAPI()->GetDevice()->GetRenderSurface());

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::RetrieveImageHandles()
{
    Array<VkImage> vk_images;
    uint32 image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(GetRenderingAPI()->GetDevice()->GetDevice(), m_handle, &image_count, nullptr);

    vk_images.Resize(image_count);

    vkGetSwapchainImagesKHR(GetRenderingAPI()->GetDevice()->GetDevice(), m_handle, &image_count, vk_images.Data());

    m_images.Resize(image_count);

    for (uint32 i = 0; i < image_count; i++)
    {
        VulkanImageRef image = MakeRenderObject<VulkanImage>(
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                m_image_format,
                Vec3u { m_extent.x, m_extent.y, 1 } });

        image->m_handle = vk_images[i];
        image->m_is_handle_owned = false;

        HYPERION_BUBBLE_ERRORS(image->Create());

        m_images[i] = std::move(image);
    }

    HYPERION_RETURN_OK;
}

#pragma endregion Swapchain

} // namespace renderer
} // namespace hyperion