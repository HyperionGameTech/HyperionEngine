#include "RxSwapchain.hpp"

#include "RxDevice.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxDefines.hpp>
#include <Core/FxPanic.hpp>
#include <Renderer/RxDeferred.hpp>

FX_SET_MODULE_NAME("RxSwapchain")

void RxSwapchain::Init(FxVec2u size, VkSurfaceKHR& surface, RxGpuDevice* device)
{
    mDevice = device;

    CreateSwapchain(size, surface);
    CreateSamplers();
    CreateSwapchainImages();
    CreateImageViews();
    CreateFramebuffers();

    bInitialized = true;
}

void RxSwapchain::CreateSwapchainImages()
{
    uint32 image_count;

    vkGetSwapchainImagesKHR(mDevice->Device, mSwapchain, &image_count, nullptr);

    FxSizedArray<VkImage> raw_images;
    raw_images.InitSize(image_count);

    vkGetSwapchainImagesKHR(mDevice->Device, mSwapchain, &image_count, raw_images.pData);

    OutputImages.InitCapacity(image_count);

    for (VkImage& raw_image : raw_images) {
        RxImage* image = OutputImages.Insert();
        image->Image = raw_image;
        image->View = nullptr;
        image->Allocation = nullptr;
        image->ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image->Format = Surface.Format;
    }
}

void RxSwapchain::CreateFramebuffers() {}

void RxSwapchain::CreateImageViews()
{
    for (int32 i = 0; i < OutputImages.Size; i++) {
        const VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = OutputImages[i].Image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = RxImageFormatUtil::ToUnderlying(Surface.Format),
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        };

        VkResult status = vkCreateImageView(mDevice->Device, &create_info, nullptr, &OutputImages[i].View);
        if (status != VK_SUCCESS) {
            FxModulePanicVulkan("Could not create swapchain image view", status);
        }
    }
}

void RxSwapchain::CreateSwapchain(FxVec2u size, VkSurfaceKHR& surface)
{
    Extent = size;

    VkSurfaceCapabilitiesKHR capabilities;
    const VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mDevice->Physical, surface, &capabilities);

    if (result != VK_SUCCESS) {
        FxModulePanicVulkan("Error retrieving surface capabilities", result);
    }

    const VkExtent2D extent = {
        .width = size.X,
        .height = size.Y,
    };

    uint32 image_count = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    FxLogInfo("Swapchain - Min:{:d}, Max:{:d}, Selected:{:d}", capabilities.minImageCount, capabilities.maxImageCount,
              image_count);

    // Retrieve the image format for the surface (the window's render target)
    {
        VkSurfaceFormatKHR surface_format = mDevice->GetSurfaceFormat();

        // For now we will enforce that RGBA16 is supported by the render device. This is the first chosen
        // if it is supported.
        FxAssert(surface_format.format == VK_FORMAT_R16G16B16A16_SFLOAT ||
                 surface_format.format == VK_FORMAT_R8G8B8A8_UNORM);

        if (surface_format.format == VK_FORMAT_R16G16B16A16_SFLOAT) {
            Surface.Format = RxImageFormat::eRGBA16_Float;
        }
        else if (surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            Surface.Format = RxImageFormat::eRGBA8_UNorm;
        }

        Surface.ColorSpace = surface_format.colorSpace;
    }

    const VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,

        .minImageCount = image_count,

        .imageFormat = RxImageFormatUtil::ToUnderlying(Surface.Format),
        .imageColorSpace = Surface.ColorSpace,

        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

        .presentMode = present_mode,
        .clipped = VK_TRUE,
        // mSwapchain is null if not initialized
        .oldSwapchain = mSwapchain,
    };

    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;

    const VkResult status = vkCreateSwapchainKHR(mDevice->Device, &create_info, nullptr, &mSwapchain);

    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Could not create swapchain", status);
    }
}

void RxSwapchain::CreateSamplers()
{
    ColorSampler.Create();

    DepthSampler.Create(RxSamplerProps {
        RxSamplerFilter::eNearest,
        RxSamplerFilter::eNearest,
        RxSamplerFilter::eNearest,
    });

    ShadowDepthSampler.Create(RxSamplerProps {
        RxSamplerFilter::eLinear,
        RxSamplerFilter::eLinear,
        RxSamplerFilter::eLinear,
        RxSamplerAddressMode::eClampToBorder,
        RxSamplerBorderColor::eFloatWhite,
        RxSamplerCompareOp::eGreater,
    });

    NormalsSampler.Create();
    LightsSampler.Create();
}

void RxSwapchain::DestroyFramebuffersAndImageViews()
{
    for (int i = 0; i < RxFramesInFlight; i++) {
        // HACK: Clears the image so that we only destroy the image view. This should be updated!
        OutputImages[i].Image = nullptr;
        OutputImages[i].DecRef();
    }


    // TODO: Add sampler cache!
    ColorSampler.Destroy();
    DepthSampler.Destroy();
    ShadowDepthSampler.Destroy();
    NormalsSampler.Destroy();
    LightsSampler.Destroy();
}

void RxSwapchain::DestroyInternalSwapchain() { vkDestroySwapchainKHR(mDevice->Device, mSwapchain, nullptr); }

void RxSwapchain::Destroy()
{
    if (!bInitialized) {
        return;
    }

    DestroyFramebuffersAndImageViews();
    // Images.Free();
    DestroyInternalSwapchain();

    bInitialized = false;
}

RxSwapchain::~RxSwapchain() { Destroy(); }
