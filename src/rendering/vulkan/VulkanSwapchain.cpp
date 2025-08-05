/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanSwapchain.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanImage.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

static const bool useSrgb = true;
static const VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

static RendererResult HandleNextFrame(
    VulkanSwapchain* swapchain,
    VulkanFrame* frame,
    uint32* index,
    bool& outNeedsRecreate)
{
    VkResult vkResult = vkAcquireNextImageKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        swapchain->GetVulkanHandle(),
        UINT64_MAX,
        frame->GetPresentSemaphores().GetWaitSemaphores()[0].Get().GetVulkanHandle(),
        VK_NULL_HANDLE,
        index);

    if (vkResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        outNeedsRecreate = true;

        return {};
    }
    else if (vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to acquire next image", int(vkResult));
    }

    return {};
}

#pragma region Swapchain

VulkanSwapchain::VulkanSwapchain()
{
}

VulkanSwapchain::~VulkanSwapchain()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE);
}

bool VulkanSwapchain::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

void VulkanSwapchain::NextFrame()
{
    m_currentFrameIndex = (m_currentFrameIndex + 1) % g_framesInFlight;
}

RendererResult VulkanSwapchain::PrepareFrame(bool& outNeedsRecreate)
{
    static const auto handleFrameResult = [](VkResult result, bool& outNeedsRecreate) -> RendererResult
    {
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            outNeedsRecreate = true;

            HYPERION_RETURN_OK;
        }

        VULKAN_CHECK(result);

        HYPERION_RETURN_OK;
    };

    const VulkanFrameRef& frame = GetCurrentFrame();

    HYP_GFX_CHECK(frame->GetFence()->WaitForGPU(true));

    HYP_GFX_CHECK(handleFrameResult(frame->GetFence()->GetLastFrameResult(), outNeedsRecreate));

    HYP_GFX_CHECK(frame->ResetFrameState());

    HYP_GFX_CHECK(HandleNextFrame(this, frame, &m_acquiredImageIndex, outNeedsRecreate));

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::PresentFrame(VulkanDeviceQueue* queue) const
{
    // Debug: ensure all images are in the PRESENT state
#ifdef HYP_DEBUG_MODE
    for (const ImageRef& image : m_images)
    {
        HYP_GFX_ASSERT(image.IsValid());
        HYP_GFX_ASSERT(image->GetResourceState() == RS_PRESENT);
    }
#endif

    const VulkanFrameRef& frame = GetCurrentFrame();

    const auto& signalSemaphores = frame->GetPresentSemaphores().GetSignalSemaphoresView();

    VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = uint32(signalSemaphores.size());
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_handle;
    presentInfo.pImageIndices = &m_acquiredImageIndex;
    presentInfo.pResults = nullptr;

    VULKAN_CHECK(vkQueuePresentKHR(queue->queue, &presentInfo));

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::Create()
{
    if (m_surface == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot initialize swapchain without a surface");
    }

    HYP_GFX_CHECK(RetrieveSupportDetails());
    HYP_GFX_CHECK(ChooseSurfaceFormat());
    HYP_GFX_CHECK(ChoosePresentMode());

    m_extent = {
        m_supportDetails.capabilities.currentExtent.width,
        m_supportDetails.capabilities.currentExtent.height
    };

    if (m_extent.x * m_extent.y == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to retrieve swapchain resolution!");
    }

    uint32 imageCount = m_supportDetails.capabilities.minImageCount + 1;

    if (m_supportDetails.capabilities.maxImageCount > 0 && imageCount > m_supportDetails.capabilities.maxImageCount)
    {
        imageCount = m_supportDetails.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_surfaceFormat.format;
    createInfo.imageColorSpace = m_surfaceFormat.colorSpace;
    createInfo.imageExtent = { m_extent.x, m_extent.y };
    createInfo.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    createInfo.imageUsage = imageUsageFlags;

    /* Graphics computations and presentation are done on separate hardware */
    const QueueFamilyIndices& qfIndices = GetRenderBackend()->GetDevice()->GetQueueFamilyIndices();

    const uint32 concurrentFamilies[] = {
        qfIndices.graphicsFamily.Get(),
        qfIndices.presentFamily.Get()
    };

    if (qfIndices.graphicsFamily.Get() != qfIndices.presentFamily.Get())
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = uint32(std::size(concurrentFamilies)); /* Two family indices(one for each process) */
        createInfo.pQueueFamilyIndices = concurrentFamilies;
    }
    else
    {
        /* Computations and presentation are done on same hardware(most scenarios) */
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     /* Optional */
        createInfo.pQueueFamilyIndices = nullptr; /* Optional */
    }

    /* For transformations such as rotations, etc. */
    createInfo.preTransform = m_supportDetails.capabilities.currentTransform;
    /* This can be used to blend with other windows in the windowing system, but we
     * simply leave it opaque.*/
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = m_presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VULKAN_CHECK_MSG(
        vkCreateSwapchainKHR(GetRenderBackend()->GetDevice()->GetDevice(), &createInfo, nullptr, &m_handle),
        "Failed to create Vulkan swapchain!");

    HYP_GFX_CHECK(RetrieveImageHandles());

    for (const ImageRef& image : m_images)
    {
        HYP_GFX_ASSERT(image != nullptr);

        if (!image->IsCreated())
        {
            HYP_GFX_CHECK(HYP_MAKE_ERROR(RendererError, "Image is not created!"));
        }

        if (image->GetResourceState() != RS_PRESENT)
        {
            HYP_GFX_CHECK(HYP_MAKE_ERROR(RendererError, "Image resource state is not PRESENT!"));
        }

        VulkanFramebufferRef framebuffer = VULKAN_CAST(m_framebuffers.PushBack(MakeRenderObject<VulkanFramebuffer>(m_extent, RenderPassStage::PRESENT)));
        framebuffer->AddAttachment(0, VulkanImageRef(image), LoadOperation::CLEAR, StoreOperation::STORE);
        HYP_GFX_CHECK(framebuffer->Create());
    }

    VulkanDeviceQueue* queue = &GetRenderBackend()->GetDevice()->GetGraphicsQueue();

    for (uint32 i = 0; i < m_frames.Size(); i++)
    {
        VkCommandPool pool = queue->commandPools[0];
        HYP_GFX_ASSERT(pool != VK_NULL_HANDLE);

        m_commandBuffers[i] = MakeRenderObject<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        m_frames[i] = MakeRenderObject<VulkanFrame>(i);

        VulkanCommandBufferRef& commandBuffer = m_commandBuffers[i];
        HYP_GFX_CHECK(commandBuffer->Create(pool));

        VulkanFrameRef& frame = m_frames[i];
        HYP_GFX_CHECK(frame->Create());
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
    SafeRelease(std::move(m_commandBuffers));

    vkDestroySwapchainKHR(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::ChooseSurfaceFormat()
{
    m_surfaceFormat = {};

    if (useSrgb)
    {
        /* look for srgb format */
        m_imageFormat = GetRenderBackend()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
            m_supportDetails,
            { { TF_RGBA8_SRGB, TF_BGRA8_SRGB } },
            [this](auto&& format)
            {
                if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return false;
                }

                m_surfaceFormat = format;

                return true;
            });

        if (m_imageFormat != TF_NONE)
        {
            HYPERION_RETURN_OK;
        }
    }

    /* look for non-srgb format */
    m_imageFormat = GetRenderBackend()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
        m_supportDetails,
        { { TF_R11G11B10F, TF_RGBA16F, TF_RGBA8 } },
        [this](auto&& format)
        {
            m_surfaceFormat = format;

            return true;
        });

    if (m_imageFormat == TF_NONE)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to find a supported surface format");
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::ChoosePresentMode()
{
    m_presentMode = HYP_ENABLE_VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::RetrieveSupportDetails()
{
    m_supportDetails = GetRenderBackend()->GetDevice()->GetFeatures().QuerySwapchainSupport(GetRenderBackend()->GetDevice()->GetRenderSurface());

    HYPERION_RETURN_OK;
}

RendererResult VulkanSwapchain::RetrieveImageHandles()
{
    Array<VkImage> vkImages;
    uint32 imageCount = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, &imageCount, nullptr);

    vkImages.Resize(imageCount);

    vkGetSwapchainImagesKHR(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, &imageCount, vkImages.Data());

    m_images.Resize(imageCount);

    for (uint32 i = 0; i < imageCount; i++)
    {
        const TextureDesc desc {
            TT_TEX2D,
            m_imageFormat,
            Vec3u { m_extent.x, m_extent.y, 1 }
        };

        VulkanImageRef image = MakeRenderObject<VulkanImage>(desc);

        image->m_handle = vkImages[i];
        image->m_isHandleOwned = false;

        HYP_GFX_CHECK(image->Create());

        m_images[i] = std::move(image);
    }

    // Transition each image to PRESENT state
    UniquePtr<SingleTimeCommands> singleTimeCommands = GetRenderBackend()->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue)
        {
            for (const ImageRef& image : m_images)
            {
                HYP_GFX_ASSERT(image.IsValid());

                renderQueue << InsertBarrier(image, RS_PRESENT);
            }
        });

    HYP_GFX_CHECK(singleTimeCommands->Execute());

    // Ensure all images are in the PRESENT state
    for (const ImageRef& image : m_images)
    {
        HYP_GFX_ASSERT(image.IsValid());
        HYP_GFX_ASSERT(image->GetResourceState() == RS_PRESENT);
    }

    HYP_LOG(RenderingBackend, Info, "Created swapchain with {} images", m_images.Size());

    HYPERION_RETURN_OK;
}

#pragma endregion Swapchain

} // namespace hyperion