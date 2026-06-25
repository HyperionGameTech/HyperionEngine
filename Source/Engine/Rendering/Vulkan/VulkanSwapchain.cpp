/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanSwapchain.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanGpuImage.hpp>
#include <Rendering/Vulkan/VulkanHelpers.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanFeatures.hpp>
#include <Rendering/Vulkan/VulkanSemaphore.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/CrashHandler.hpp>

#include <Core/Debug/Debug.hpp>

// for EnumToString
#include <Core/Reflection/Enum.hpp>

#include <Framework/CVarManager.hpp>

#include <VulkanSwapchain.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

extern CVar<bool> g_cvEnableVSync;

static constexpr bool UseSRGBFormat = true;
static constexpr bool UseHDRFormat = false;

static constexpr VkImageUsageFlags ImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

static RendererResult AcquireNextImage(
    VulkanSwapchain* swapchain,
    VulkanFrame* frame,
    uint32* index,
    bool* pOutNeedsRecreate)
{
    VulkanSemaphore* semaphore = frame->GetImageAvailableSemaphore(swapchain);
    Assert(semaphore != nullptr && semaphore->IsCreated());

    VkResult vkResult = vkAcquireNextImageKHR(
        RI.GetDevice()->GetDevice(),
        swapchain->GetVulkanHandle(),
        UINT64_MAX,
        semaphore->GetVulkanHandle(),
        VK_NULL_HANDLE,
        index);

    if (pOutNeedsRecreate != nullptr && vkResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        *pOutNeedsRecreate = true;
    }

    if (vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to acquire next image", int(vkResult));
    }

    // After vkAcquireNextImageKHR, the acquired image is in VK_IMAGE_LAYOUT_UNDEFINED.
    // Reset the tracked state so subsequent barriers use the correct oldLayout.
    swapchain->GetImages()[*index]->SetResourceState(RS_UNDEFINED);

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
    m_images.Clear();
    m_presentSemaphores.Clear();
    m_framebuffers.Clear();

    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
                                                      {
                                                          vkDestroySwapchainKHR(RI.GetDevice()->GetDevice(), handle, nullptr);
                                                      }));
    }

    m_handle = VK_NULL_HANDLE;
    m_surface = VK_NULL_HANDLE;
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

        // if recreation failed our handle will be VK_NULL_HANDLE
        // this can happen when closing the window
        if (m_handle == VK_NULL_HANDLE)
        {
            return;
        }
    }

    RendererResult result = AcquireNextImage(this, frame, &m_acquiredImageIndex, &m_needsRecreate);

    if (m_needsRecreate)
    {
        Recreate();

        frame->RecreateSemaphores(this);

        result = AcquireNextImage(this, frame, &m_acquiredImageIndex, &m_needsRecreate);
    }

    Assert(result, "Failed to acquire next swapchain image: {}", result.GetError().GetMessage());
}

void VulkanSwapchain::PresentFrame(VulkanFrame* frame, VulkanDeviceQueue* queue)
{
    AssertOnThread(g_renderThread);

    // Debug: ensure all images are in the PRESENT state
#if HYP_DEBUG_MODE
    for (VulkanGpuImage* image : m_images)
    {
        Assert(image->GetResourceState() == RS_PRESENT);
    }
#endif

    VulkanSemaphore* presentSemaphore = GetCurrentPresentSemaphore();
    Assert(presentSemaphore != nullptr && presentSemaphore->IsCreated());

    VkSemaphore signalSemaphores[] = { presentSemaphore->GetVulkanHandle() };

    VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_handle;
    presentInfo.pImageIndices = &m_acquiredImageIndex;
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(queue->queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        HYP_LOG(RenderingBackend, Verbose, "Got out of date swapchain present result ({}), calling Recreate()", result);

        Recreate();
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        CrashHandler::Dump();

        HYP_FAIL("Failed to present swapchain image: {}", int(result));
    }
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

    m_supportDetails = RI.GetDevice()->GetFeatures().QuerySwapchainSupport(m_surface);

    CheckResultOrReturn(ChooseSurfaceFormat());

    m_presentMode = g_cvEnableVSync.Get() ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    HYP_LOG(RenderingBackend, Verbose, "Vulkan swapchain m_extent = {}", m_extent);

    const Vec2u nativeExtent {
        m_supportDetails.capabilities.currentExtent.width,
        m_supportDetails.capabilities.currentExtent.height
    };

    if (nativeExtent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to retrieve native surface resolution!");
    }

    HYP_LOG(RenderingBackend, Verbose, "Vulkan native swapchain resolution: {}", nativeExtent);

    if (m_extent.Volume() == 0)
    {
        m_extent = nativeExtent;
    }

    const Vec2u maxExtent {
        m_supportDetails.capabilities.maxImageExtent.width,
        m_supportDetails.capabilities.maxImageExtent.height
    };

    const Vec2u minExtent {
        m_supportDetails.capabilities.minImageExtent.width,
        m_supportDetails.capabilities.minImageExtent.height
    };

    m_extent = MathUtil::Min(maxExtent, m_extent);
    m_extent = MathUtil::Max(minExtent, m_extent);

    Assert(m_extent.Volume() != 0);

    HYP_LOG(RenderingBackend, Verbose, "Using Vulkan swapchain resolution {}", m_extent);

    uint32 imageCount = m_supportDetails.capabilities.minImageCount + 1;

    if (m_supportDetails.capabilities.maxImageCount > 0 && imageCount > m_supportDetails.capabilities.maxImageCount)
    {
        imageCount = m_supportDetails.capabilities.maxImageCount;
    }

#if HYP_ANDROID || HYP_IOS
    // reduce the number of swapchain images on mobile gpus to save bandwidth
    imageCount = MathUtil::Min(imageCount, NumFramesInFlight);
    imageCount = MathUtil::Max(imageCount, m_supportDetails.capabilities.minImageCount);
#endif

    HYP_LOG(RenderingBackend, Verbose, "Creating Vulkan swapchain with {} images", imageCount);
    HYP_LOG(RenderingBackend, Verbose, "Min image count: {}, max image count: {}, desired image count: {}", m_supportDetails.capabilities.minImageCount, m_supportDetails.capabilities.maxImageCount, imageCount);

    VkSwapchainCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_surfaceFormat.format;
    createInfo.imageColorSpace = m_surfaceFormat.colorSpace;
    createInfo.imageExtent = { m_extent.x, m_extent.y };
    createInfo.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    createInfo.imageUsage = ImageUsageFlags;

    /* Graphics computations and presentation are done on separate hardware */
    const QueueFamilyIndices& qfIndices = RI.GetDevice()->GetQueueFamilyIndices();

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

    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
#if HYP_ANDROID
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
#else
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
#endif
    createInfo.presentMode = m_presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = m_oldHandle;

    VkResult result = vkCreateSwapchainKHR(RI.GetDevice()->GetDevice(), &createInfo, nullptr, &m_handle);

    if (result != VK_SUCCESS)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create swapchain!", int(result));
    }

    AssertDebug(m_images.Empty());

    CheckResultOrReturn(RetrieveImageHandles());

    AssertDebug(m_images.Any());
    AssertDebug(m_framebuffers.Empty());

    HYP_LOG(RenderingBackend, Verbose, "Creating {} swapchain framebuffers with extent and format: {}",
            m_images.Size(), m_extent, EnumToString(m_images[0]->GetTextureFormat()));

    for (const VulkanGpuImageRef& image : m_images)
    {
        AssertDebug(image && image->IsCreated());
        AssertDebug(image->GetResourceState() == RS_PRESENT);

        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_extent;
        framebufferDesc.renderPassMode = RenderPassMode::Present;

        VulkanFramebufferRef framebuffer = MakeHandle<VulkanFramebuffer>(framebufferDesc);
        framebuffer->AddAttachment(
            0,
            AttachmentDesc {
                TextureType::Texture2D,
                image->GetTextureFormat(),
                LoadOperation::CLEAR,
                StoreOperation::STORE },
            RI.MakeImageView(image));

        CheckResultOrReturn(framebuffer->Create());

        m_framebuffers.PushBack(framebuffer);
    }

    // Create present semaphores
    m_presentSemaphores.Resize(m_images.Size());
    for (uint32 i = 0; i < m_presentSemaphores.Size(); i++)
    {
        m_presentSemaphores[i] = MakeHandle<VulkanSemaphore>();
        CheckResultOrReturn(m_presentSemaphores[i]->Create());
    }

    m_needsRecreate = false;

    return {};
}

void VulkanSwapchain::SetExtent(Vec2u newExtent)
{
    if (m_extent == newExtent || newExtent == Vec2u::Zero())
    {
        return;
    }

    m_extent = newExtent;

    if (!IsCreated())
    {
        return;
    }

    m_needsRecreate = true;
}

void VulkanSwapchain::Recreate()
{
    HYP_LOG(RenderingBackend, Verbose, "Recreating Vulkan swapchain {} with new extent: {}", Id(), m_extent);

    Array<VulkanGpuImageRef, VulkanAllocator> oldImages = std::move(m_images);
    Array<VulkanFramebufferRef, VulkanAllocator> oldFramebuffers = std::move(m_framebuffers);
    Array<VulkanSemaphoreRef, VulkanAllocator> oldPresentSemaphores = std::move(m_presentSemaphores);

    m_oldHandle = m_handle;
    m_handle = VK_NULL_HANDLE; // so Create() knows it's a new swapchain

    m_acquiredImageIndex = ~0u;

    RendererResult createResult = Create();

    if (createResult)
    {
        if (m_oldHandle != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(
                RI.GetDevice()->GetDevice(),
                m_oldHandle,
                nullptr);

            m_oldHandle = VK_NULL_HANDLE;
        }

        // cleanup old resources
        oldFramebuffers.Clear();
        oldImages.Clear();
        oldPresentSemaphores.Clear();
    }
    else
    {
        HYP_LOG(RenderingBackend, Error, "Failed to recreate Vulkan swapchain: {}", createResult.GetError().GetMessage());

        // restore previous state
        m_handle = m_oldHandle;
        m_oldHandle = VK_NULL_HANDLE;

        m_images = std::move(oldImages);
        m_framebuffers = std::move(oldFramebuffers);
        m_presentSemaphores = std::move(oldPresentSemaphores);

        m_needsRecreate = true; // try again next frame
    }
}

RendererResult VulkanSwapchain::ChooseSurfaceFormat()
{
    m_surfaceFormat = {};

    if (UseHDRFormat)
    {
        /* look for hdr format */
        m_imageFormat = RI.GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
            m_supportDetails,
            { { TextureFormat::R10G10B10A2, TextureFormat::R11G11B10F, TextureFormat::RGBA16F } },
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

        if (m_imageFormat != InvalidTextureFormat)
        {
            HYP_LOG(RenderingBackend, Info, "Found supported surface format for swapchain (HDR): {}", EnumToString(m_imageFormat));

            return {};
        }
    }

    if (UseSRGBFormat)
    {
        /* look for srgb format */
        m_imageFormat = RI.GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
            m_supportDetails,
            { { TextureFormat::RGBA8_SRGB, TextureFormat::BGRA8_SRGB } },
            [this](VkSurfaceFormatKHR format)
            {
                if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return false;
                }

                m_surfaceFormat = format;

                return true;
            });

        if (m_imageFormat != InvalidTextureFormat)
        {
            HYP_LOG(RenderingBackend, Info, "Found supported surface format for swapchain (sRGB): {}", EnumToString(m_imageFormat));

            return {};
        }
    }

    /* look for non-srgb format */
    m_imageFormat = RI.GetDevice()->GetFeatures().FindSupportedSurfaceFormat(
        m_supportDetails,
        { { TextureFormat::R11G11B10F, TextureFormat::RGBA16F, TextureFormat::RGBA8 } },
        [this](auto&& format)
        {
            m_surfaceFormat = format;

            return true;
        });

    if (m_imageFormat != InvalidTextureFormat)
    {
        HYP_LOG(RenderingBackend, Info, "Found supported surface format for swapchain (non-sRGB): {}", EnumToString(m_imageFormat));

        return {};
    }

    return HYP_MAKE_ERROR(RendererError, "Failed to find a supported surface format!");
}

RendererResult VulkanSwapchain::RetrieveImageHandles()
{
    Array<VkImage> vkImages;
    uint32 imageCount = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(RI.GetDevice()->GetDevice(), m_handle, &imageCount, nullptr);

    vkImages.Resize(imageCount);

    vkGetSwapchainImagesKHR(RI.GetDevice()->GetDevice(), m_handle, &imageCount, vkImages.Data());

    m_images.Resize(imageCount);

    for (uint32 i = 0; i < imageCount; i++)
    {
        const TextureDesc desc {
            TextureType::Texture2D,
            m_imageFormat,
            Vec3u { m_extent.x, m_extent.y, 1 }
        };

        VulkanGpuImageRef image = MakeHandle<VulkanGpuImage>(desc);

#if HYP_DEBUG_MODE
        image->SetDebugName(NAME_FMT("SwapchainImage_{}", i));
#endif

        image->m_handle = vkImages[i];
        image->m_isHandleOwned = false;

        CheckResultOrReturn(image->Create());

        m_images[i] = std::move(image);
    }

    // Transition each image to PRESENT state immediately
    UniquePtr<SingleTimeCommands> singleTimeCommands = RI.GetSingleTimeCommands();

    singleTimeCommands->Push([&](CommandRecorder& cr)
                             {
                                 for (const VulkanGpuImageRef& image : m_images)
                                 {
                                     Assert(image.IsValid());

                                     cr << InsertBarrier(image, RS_PRESENT);
                                 }
                             });

    CheckResultOrReturn(singleTimeCommands->Execute());

#if HYP_DEBUG_MODE
    // Ensure all images are in the PRESENT state
    for (VulkanGpuImage* image : m_images)
    {
        Assert(image->GetResourceState() == RS_PRESENT);
    }
#endif

    return {};
}

#pragma endregion Swapchain

} // namespace Hyperion
