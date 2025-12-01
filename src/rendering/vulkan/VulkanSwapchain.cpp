/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include "vulkan/vulkan_core.h"
#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanSwapchain.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/debug/Debug.hpp>

// for EnumToString
#include <core/reflection/Enum.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <VulkanSwapchain.generated.inl>

namespace hyperion {

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

static constexpr bool UseSrgbFormat = true;
static constexpr bool UseHdrFormat = false;
static constexpr VkImageUsageFlags ImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

static RendererResult AcquireNextImage(
    VulkanSwapchain* swapchain,
    VulkanFrame* frame,
    uint32* index,
    bool& outNeedsRecreate)
{
    VkResult vkResult = vkAcquireNextImageKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        swapchain->GetVulkanHandle(),
        UINT64_MAX,
        frame->GetImageAvailableSemaphore()->GetVulkanHandle(),
        VK_NULL_HANDLE,
        index);

    if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
    {
        outNeedsRecreate = true;
    }

    if (vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to acquire next image", int(vkResult));
    }

    return {};
}

#pragma region Swapchain

VulkanSwapchain::VulkanSwapchain(VkSurfaceKHR surface, const Vec2u& extent)
    : SwapchainBase(extent),
      m_handle(VK_NULL_HANDLE),
      m_oldHandle(VK_NULL_HANDLE),
      m_surface(surface),
      m_surfaceFormat(),
      m_presentMode(),
      m_supportDetails()
{
}

VulkanSwapchain::~VulkanSwapchain()
{
    if (m_handle == VK_NULL_HANDLE)
    {
        return;
    }

    SafeDelete(std::move(m_images));
    SafeDelete(std::move(m_framebuffers));

    vkDestroySwapchainKHR(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;
}

bool VulkanSwapchain::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

void VulkanSwapchain::PrepareForFrame(VulkanFrame* frame)
{
    if (m_needsRecreate)
    {
        Recreate();
    }

    RendererResult result = AcquireNextImage(this, frame, &m_acquiredImageIndex, m_needsRecreate);

    if (m_needsRecreate)
    {
        Recreate();

        frame->RecreateSemaphores();

        result = AcquireNextImage(this, frame, &m_acquiredImageIndex, m_needsRecreate);
    }

    Assert(result, "Failed to acquire next swapchain image: {}", result.GetError().GetMessage());
}

void VulkanSwapchain::PresentFrame(VulkanFrame* frame, VulkanDeviceQueue* queue) const
{
    AssertOnThread(g_renderThread);

    // Debug: ensure all images are in the PRESENT state
#ifdef HYP_DEBUG_MODE
    for (VulkanGpuImage* image : m_images)
    {
        HYP_GFX_ASSERT(image->GetResourceState() == RS_PRESENT);
    }
#endif

    AssertDebug(!m_needsRecreate); // should have been handled before present

    VkSemaphore signalSemaphores[] = { frame->GetRenderFinishedSemaphore()->GetVulkanHandle() };

    VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_handle;
    presentInfo.pImageIndices = &m_acquiredImageIndex;
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(queue->queue, &presentInfo);

    if (result != VK_SUCCESS)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to present swapchain image: {}", int(result));
    }

    frame->ResetRenderPassStates();
}

RendererResult VulkanSwapchain::Create()
{
    if (IsCreated())
    {
        return {};
    }

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
    createInfo.imageUsage = ImageUsageFlags;

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
    createInfo.oldSwapchain = m_oldHandle;

    VkResult result = vkCreateSwapchainKHR(GetRenderBackend()->GetDevice()->GetDevice(), &createInfo, nullptr, &m_handle);

    if (result != VK_SUCCESS)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create swapchain!", int(result));
    }

    AssertDebug(m_images.Empty());

    HYP_GFX_CHECK(RetrieveImageHandles());

    AssertDebug(m_images.Any());
    AssertDebug(m_framebuffers.Empty());

    HYP_LOG(RenderingBackend, Info, "Creating {} swapchain framebuffers with extent and format: {}",
        m_images.Size(), m_extent, EnumToString(m_images[0]->GetTextureFormat()));

    for (const VulkanGpuImageRef& image : m_images)
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

        VulkanFramebufferRef& framebuffer = m_framebuffers.PushBack(CreateObject<VulkanFramebuffer>(m_extent, RTT_PRESENT));
        framebuffer->AddAttachment(0, image, LoadOperation::CLEAR, StoreOperation::STORE);
        HYP_GFX_CHECK(framebuffer->Create());
    }

    return {};
}

void VulkanSwapchain::Resize(Vec2u newExtent)
{
    if (m_extent == newExtent)
    {
        return;
    }

    if (!IsCreated())
    {
        m_extent = newExtent;

        return;
    }

    m_needsRecreate = true;

    // Mutex::Guard* pGuard = nullptr;
    // HYP_DEFER({ if (pGuard) delete pGuard; });

    //// safely destroy the old swapchain after it's no longer in use
    // VkSwapchainKHR* pSwapchain = GetSafeDeleterInstance()->AllocCustom<VkSwapchainKHR>([](void* ptr)
    //     {
    //         VkSwapchainKHR* pSwapchain = reinterpret_cast<VkSwapchainKHR*>(ptr);

    //        vkDestroySwapchainKHR(
    //            GetRenderBackend()->GetDevice()->GetDevice(),
    //            *pSwapchain,
    //            nullptr);
    //    },
    //    &pGuard);

    //*pSwapchain = m_oldHandle;
    // m_oldHandle = VK_NULL_HANDLE;
}

void VulkanSwapchain::Recreate()
{
    if (!IsCreated())
    {
        m_needsRecreate = false;

        return;
    }

    HYP_LOG(RenderingBackend, Info, "Recreating Vulkan swapchain {} with new extent: {}", Id(), m_extent);

    Array<VulkanGpuImageRef> oldImages = std::move(m_images);
    Array<VulkanFramebufferRef> oldFramebuffers = std::move(m_framebuffers);

    m_oldHandle = m_handle;
    m_handle = VK_NULL_HANDLE; // so Create() knows it's a new swapchain

    RendererResult createResult = Create();
    Assert(createResult, "Failed to recreate swapchain during resize: {}", createResult.GetError().GetMessage());

    HYP_LOG_TEMP("Recreated swapchain {}, old swapchain was {}", (void*)m_handle, (void*)m_oldHandle);

    // we can now destroy the old swapchain
    vkDestroySwapchainKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        m_oldHandle,
        nullptr);

    m_oldHandle = VK_NULL_HANDLE;

    // cleanup old resources
    oldFramebuffers.Clear();
    oldImages.Clear();

    OnRecreated();

    m_needsRecreate = false;
}

RendererResult VulkanSwapchain::ChooseSurfaceFormat()
{
    m_surfaceFormat = {};

    if (UseHdrFormat)
    {
        /* look for hdr format */
        m_imageFormat = GetRenderBackend()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
            m_supportDetails,
            { { TF_R10G10B10A2, TF_R11G11B10F, TF_RGBA16F } },
            [this](VkSurfaceFormatKHR format)
            {
                if (format.colorSpace != VK_COLOR_SPACE_HDR10_ST2084_EXT && format.colorSpace != VK_COLOR_SPACE_BT2020_LINEAR_EXT && format.colorSpace != VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
                {
                    return false;
                }

                m_surfaceFormat = format;

                if (format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
                {
                    m_isPqHdr = true;
                }

                return true;
            });

        if (m_imageFormat != TF_NONE)
        {
            HYP_LOG(RenderingBackend, Info, "Found supported surface format for swapchain (HDR): {}", EnumToString(m_imageFormat));

            return {};
        }
    }

    if (UseSrgbFormat)
    {
        /* look for srgb format */
        m_imageFormat = GetRenderBackend()->GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
            m_supportDetails,
            { { TF_RGBA8_SRGB, TF_BGRA8_SRGB } },
            [this](VkSurfaceFormatKHR format)
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
            HYP_LOG(RenderingBackend, Info, "Found supported surface format for swapchain (sRGB): {}", EnumToString(m_imageFormat));

            return {};
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

    if (m_imageFormat != TF_NONE)
    {
        HYP_LOG(RenderingBackend, Info, "Found supported surface format for swapchain (non-sRGB): {}", EnumToString(m_imageFormat));

        return {};
    }

    return HYP_MAKE_ERROR(RendererError, "Failed to find a supported surface format!");
}

RendererResult VulkanSwapchain::ChoosePresentMode()
{
    m_presentMode = HYP_ENABLE_VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    return {};
}

RendererResult VulkanSwapchain::RetrieveSupportDetails()
{
    m_supportDetails = GetRenderBackend()->GetDevice()->GetFeatures().QuerySwapchainSupport(m_surface);

    return {};
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

        VulkanGpuImageRef image = CreateObject<VulkanGpuImage>(desc);

#ifdef HYP_DEBUG_MODE
        image->SetDebugName(NAME_FMT("SwapchainImage_{}", i));
#endif

        image->m_handle = vkImages[i];
        image->m_isHandleOwned = false;

        HYP_GFX_CHECK(image->Create());

        m_images[i] = std::move(image);
    }

    // Transition each image to PRESENT state immediately
    UniquePtr<SingleTimeCommands> singleTimeCommands = GetRenderBackend()->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue)
        {
            for (const VulkanGpuImageRef& image : m_images)
            {
                HYP_GFX_ASSERT(image.IsValid());

                renderQueue << InsertBarrier(image, RS_PRESENT);
            }
        });

    HYP_GFX_CHECK(singleTimeCommands->Execute());

#ifdef HYP_DEBUG_MODE
    // Ensure all images are in the PRESENT state
    for (VulkanGpuImage* image : m_images)
    {
        HYP_GFX_ASSERT(image->GetResourceState() == RS_PRESENT);
    }
#endif

    return {};
}

#pragma endregion Swapchain

} // namespace hyperion
