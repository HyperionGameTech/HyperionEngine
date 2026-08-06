/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanGpuImage.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanFeatures.hpp>
#include <Rendering/Vulkan/VulkanGraphicsPipeline.hpp>
#include <Rendering/Vulkan/VulkanHelpers.hpp>
#include <Rendering/Vulkan/VulkanResult.hpp>

#include <Rendering/CommandRecorder.hpp>

#include <Util/Img/ImageUtil.hpp>

#include <Core/Utilities/Pair.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Debug/Debug.hpp>

#include <Vulkan/vulkan.h>

#include <VulkanGpuImage.generated.inl>

namespace Hyperion {

static constexpr size_t MaxImageBytes = 256 * 1024 * 1024; // 256 MiB - cannot be larger than a block in our blob storage system

extern VulkanRenderInterface RI;

#pragma region VulkanGpuImage

VulkanGpuImage::VulkanGpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags)
    : GpuImageBase(textureDesc, flags)
{
    m_size = textureDesc.GetByteSize();
}

VulkanGpuImage::~VulkanGpuImage()
{
    if (IsCreated())
    {
        if (m_allocation != VK_NULL_HANDLE)
        {
            Assert(m_isHandleOwned, "If allocation is not VK_NULL_HANDLE, is_handle_owned should be true");

            EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle, allocation = m_allocation]() -> void
                {
                    vmaDestroyImage(RI.GetDevice()->GetVmaAllocator(), handle, allocation);
                }));

            m_allocation = VK_NULL_HANDLE;
        }

        m_handle = VK_NULL_HANDLE;

        // reset back to default
        m_isHandleOwned = true;

        m_resourceState = RS_UNDEFINED;
        m_stencilState = RS_UNDEFINED;
        m_subResourceStates.Clear();
    }
}

bool VulkanGpuImage::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

bool VulkanGpuImage::IsOwned() const
{
    return m_isHandleOwned;
}

RendererResult VulkanGpuImage::GenerateMipmaps(VulkanCommandBuffer* commandBuffer)
{
    if (m_handle == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot generate mipmaps on uninitialized image");
    }

    const uint16 numLayers = NumArrayLayers();
    const uint8 numMipmaps = NumMips();

    for (uint16 face = 0; face < numLayers; face++)
    {
        for (int32 i = 1; i < int32(numMipmaps + 1); i++)
        {
            const int mipWidth = int(helpers::MipmapSize(m_textureDesc.extent.x, i)),
                      mipHeight = int(helpers::MipmapSize(m_textureDesc.extent.y, i)),
                      mipDepth = int(helpers::MipmapSize(m_textureDesc.extent.z, i));

            /* Memory barrier for transfer - note that after generating the mipmaps,
                we'll still need to transfer into a layout primed for reading from shaders. */

            const ImageSubResource src {
                .baseMipLevel = uint8(i - 1),
                .numLevels = 1,
                .baseArrayLayer = face,
                .numLayers = 1
            };

            const ImageSubResource dst {
                .baseMipLevel = uint8(i),
                .numLevels = 1,
                .baseArrayLayer = src.baseArrayLayer,
                .numLayers = 1
            };

            InsertBarrier(
                commandBuffer,
                src,
                RS_COPY_SRC,
                ShaderModuleType::None);

            AssertDebug(GetSubResourceState(dst) == RS_COPY_DST);

            if (i == int32(numMipmaps))
            {
                if (face == numLayers - 1)
                {
                    /* all individual subresources have been set so we mark the whole
                     * resource as being int his state */
                    SetResourceState(RS_COPY_SRC);
                }

                break;
            }

            VkImageAspectFlags aspectFlagBits = 0;

            if (TextureUtils::IsDepthFormat(GetTextureFormat()))
            {
                aspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

                if (TextureUtils::HasStencilComponent(GetTextureFormat()))
                {
                    aspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            }
            else
            {
                aspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
            }

            /* Blit src -> dst */
            const VkImageBlit blit {
                .srcSubresource = {
                    .aspectMask = aspectFlagBits,
                    .mipLevel = src.baseMipLevel,
                    .baseArrayLayer = src.baseArrayLayer,
                    .layerCount = src.numLayers
                },
                .srcOffsets = { { 0, 0, 0 }, { int32(helpers::MipmapSize(m_textureDesc.extent.x, i - 1)), int32(helpers::MipmapSize(m_textureDesc.extent.y, i - 1)), int32(helpers::MipmapSize(m_textureDesc.extent.z, i - 1)) } },
                .dstSubresource = { .aspectMask = aspectFlagBits, .mipLevel = dst.baseMipLevel, .baseArrayLayer = dst.baseArrayLayer, .layerCount = dst.numLayers },
                .dstOffsets = { { 0, 0, 0 }, { mipWidth, mipHeight, mipDepth } }
            };

            vkCmdBlitImage(
                commandBuffer->GetVulkanHandle(),
                m_handle,
                GetVkImageLayout(RS_COPY_SRC),
                m_handle,
                GetVkImageLayout(RS_COPY_DST),
                1, &blit,
                m_textureDesc.IsDepthStencil() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR // TODO: base on filter mode
            );
        }
    }

    return {};
}

RendererResult VulkanGpuImage::Create()
{
    if (IsCreated())
    {
        return {};
    }

    return Create(RS_UNDEFINED);
}

RendererResult VulkanGpuImage::Create(ResourceState initialState)
{
    if (IsCreated())
    {
        return {};
    }

    if (!m_isHandleOwned)
    {
        Assert(m_handle != VK_NULL_HANDLE, "If m_isHandleOwned is false, the image handle must not be VK_NULL_HANDLE.");

        return {};
    }

    if (GetByteSize() > MaxImageBytes)
    {
        return HYP_MAKE_ERROR(RendererError, "Image size exceeds maximum supported size of {} bytes", 0, MaxImageBytes);
    }

    const Vec3u extent = GetExtent();

    const TextureFormat format = GetTextureFormat();
    const TextureType type = GetType();

    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];
    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool isRWTexture = m_textureDesc.imageUsage[IU_STORAGE];
    const bool isExternalMemory = m_textureDesc.imageUsage[IU_EXTERNAL];

    const bool isBlended = m_textureDesc.IsBlended();
    const bool isSrgb = m_textureDesc.IsSrgb();

    const bool hasMipmaps = m_textureDesc.HasMipMaps();
    const uint32 numMipmaps = m_textureDesc.NumMips();
    const uint32 numLayers = m_textureDesc.NumArrayLayers();

    if (extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    VkImageLayout initialLayout = GetVkImageLayout(
        initialState,
        isDepthStencil && isAttachmentTexture,
        /* onlyDepth */ false,
        /* onlyStencil */ false);

    VkFormat vkFormat = ToVkFormat(format);
    VkImageType vkImageType = ToVkImageType(type);
    VkImageCreateFlags vkImageCreateFlags = 0;
    VkFormatFeatureFlags vkFormatFeatures = 0;
    VkImageFormatProperties vkImageFormatProperties {};

    m_tiling = VK_IMAGE_TILING_OPTIMAL;
    m_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (isAttachmentTexture)
    {
        m_usageFlags |= (isDepthStencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (isRWTexture)
    {
        m_usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else
    {
        m_usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (hasMipmaps)
    {
        if (!m_textureDesc.HasStoredMips())
        {
            /* Runtime mipmapped image needs linear blitting. */
            vkFormatFeatures |= VK_FORMAT_FEATURE_BLIT_DST_BIT
                | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

            m_usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        switch (GetMinFilterMode())
        {
        case TFM_LINEAR: // fallthrough
        case TFM_LINEAR_MIPMAP:
            vkFormatFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case TFM_MINMAX_MIPMAP:
            vkFormatFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        default:
            break;
        }
    }

    if (isBlended)
    {
        HYP_LOG(RenderingBackend, Verbose, "Image requires blending, enabling format flag...");

        vkFormatFeatures |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (m_textureDesc.IsTextureCube() || m_textureDesc.IsTextureCubeArray())
    {
        HYP_LOG(RenderingBackend, Verbose, "Creating cubemap, enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.");

        vkImageCreateFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    RendererResult formatSupportResult = RI.GetDevice()->GetFeatures().GetImageFormatProperties(
        vkFormat,
        vkImageType,
        m_tiling,
        m_usageFlags,
        vkImageCreateFlags,
        &vkImageFormatProperties);

    if (!formatSupportResult)
    {
        CheckResultOrReturn(formatSupportResult);
    }

    const QueueFamilyIndices& qfIndices = RI.GetDevice()->GetQueueFamilyIndices();
    const uint32 imageFamilyIndices[] = { qfIndices.graphicsFamily.Get(), qfIndices.computeFamily.Get() };

    VkImageCreateInfo imageInfo { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = vkImageType;
    imageInfo.extent.width = extent.x;
    imageInfo.extent.height = extent.y;
    imageInfo.extent.depth = extent.z;
    imageInfo.mipLevels = numMipmaps;
    imageInfo.arrayLayers = numLayers;
    imageInfo.format = vkFormat;
    imageInfo.tiling = m_tiling;
    imageInfo.initialLayout = initialLayout;
    imageInfo.usage = m_usageFlags;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = vkImageCreateFlags;
    imageInfo.pQueueFamilyIndices = imageFamilyIndices;
    imageInfo.queueFamilyIndexCount = uint32(std::size(imageFamilyIndices));

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkExternalMemoryImageCreateInfoKHR externalMemoryImageCreateInfo;

    if (isExternalMemory)
    {
#ifdef _WIN32
        constexpr VkExportMemoryAllocateInfoKHR ExportAllocInfo {
            .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
            .pNext = VK_NULL_HANDLE,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
        };
#else
        constexpr VkExportMemoryAllocateInfoKHR ExportAllocInfo {
            .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
            .pNext = VK_NULL_HANDLE,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR
        };
#endif

        uint32_t memTypeIndex;

        VkResult res = vmaFindMemoryTypeIndexForImageInfo(
            RI.GetDevice()->GetVmaAllocator(),
            &imageInfo, &allocInfo, &memTypeIndex);

        VULKAN_CHECK(res);

        VmaPoolCreateInfo poolCreateInfo {};
        poolCreateInfo.memoryTypeIndex = memTypeIndex;
        poolCreateInfo.pMemoryAllocateNext = (void*)&ExportAllocInfo;

        VmaPool pool;
        res = vmaCreatePool(RI.GetDevice()->GetVmaAllocator(), &poolCreateInfo, &pool);
        VULKAN_CHECK(res); //// \todo Have to destroy this pool later!

        allocInfo.pool = pool;

        externalMemoryImageCreateInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR };
        externalMemoryImageCreateInfo.handleTypes = ExportAllocInfo.handleTypes;

        imageInfo.pNext = &externalMemoryImageCreateInfo;
    }

    VULKAN_CHECK_MSG(
        vmaCreateImage(
            RI.GetDevice()->GetVmaAllocator(),
            &imageInfo,
            &allocInfo,
            &m_handle,
            &m_allocation,
            nullptr),
        "Failed to create gpu image!");
#ifdef HYP_RHI_DEBUG_NAMES
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    return {};
}

RendererResult VulkanGpuImage::Resize(const Vec3u& extent)
{
    if (extent == m_textureDesc.extent)
    {
        return {};
    }

    if (extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    m_textureDesc.extent = extent;

    const uint32 newSize = m_textureDesc.GetByteSize();

    if (newSize != m_size)
    {
        m_size = newSize;
    }

    const ResourceState previousResourceState = m_resourceState;

    if (IsCreated())
    {
        if (!m_isHandleOwned)
        {
            return HYP_MAKE_ERROR(RendererError, "Cannot resize non-owned image");
        }

        // destroy and recreate
        if (m_allocation != VK_NULL_HANDLE)
        {
            EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle, allocation = m_allocation]()
                {
                    vmaDestroyImage(RI.GetDevice()->GetVmaAllocator(), handle, allocation);
                }));

            m_allocation = VK_NULL_HANDLE;
        }

        m_handle = VK_NULL_HANDLE;
        m_resourceState = RS_UNDEFINED;
        m_stencilState = RS_UNDEFINED;
        m_subResourceStates.Clear();

        CheckResultOrReturn(Create());

        if (previousResourceState != RS_UNDEFINED)
        {
            SetResourceState(RS_UNDEFINED);

            VulkanFrame* frame = RI.GetCurrentFrame();
            CommandRecorder& cr = frame->cr;
            cr << ::Hyperion::InsertBarrier(this, previousResourceState);
        }
    }

    return {};
}

void VulkanGpuImage::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    ResourceState newState,
    ShaderModuleType shaderModuleType,
    bool onlyDepth,
    bool onlyStencil)
{
    AssertDebug(!commandBuffer->IsInRenderPass());

    // entire image
    ImageSubResource subResource {};
    subResource.baseMipLevel = 0;
    subResource.numLevels = NumMips();
    subResource.baseArrayLayer = 0;
    subResource.numLayers = NumArrayLayers();

    InsertBarrier(
        commandBuffer,
        subResource,
        newState,
        shaderModuleType,
        onlyDepth,
        onlyStencil);
}

void VulkanGpuImage::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    const ImageSubResource& subResource,
    ResourceState newState,
    ShaderModuleType shaderModuleType,
    bool onlyDepth,
    bool onlyStencil)
{
    AssertDebug(newState != RS_UNDEFINED && newState != RS_PRE_INITIALIZED);
    AssertDebug(!commandBuffer->IsInRenderPass());

    if (m_handle == VK_NULL_HANDLE)
    {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to insert a resource barrier but image was not defined");

        return;
    }

    AssertDebug((subResource.baseArrayLayer + subResource.numLayers) <= NumArrayLayers()
        || (subResource.baseArrayLayer == 0 && subResource.numLayers == uint16(-1)));

    AssertDebug((subResource.baseMipLevel + subResource.numLevels) <= NumMips()
        || (subResource.baseMipLevel == 0 && subResource.numLevels == uint8(-1)));

    const uint16 maxArrayLayers = uint16(subResource.baseArrayLayer + MathUtil::Min(subResource.numLayers, NumArrayLayers()));
    const uint8 maxMipLevels = uint8(subResource.baseMipLevel + MathUtil::Min(subResource.numLevels, NumMips()));

    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool hasStencil = TextureUtils::HasStencilComponent(m_textureDesc.format);


    // can only use these if we actually do have a stencil component,
    // otherwise use default/main path
    onlyDepth &= hasStencil;
    onlyStencil &= hasStencil;

    ResourceState currResourceState = m_resourceState;
    const ResourceState currStencilState = m_stencilState;

    if (HasSubResourceStates())
    {
        currResourceState = RS_UNDEFINED;

        bool firstSubResource = true;
        bool breakLoop = false;

        for (uint8 mipLevel = subResource.baseMipLevel; mipLevel < maxMipLevels; mipLevel++)
        {
            if (breakLoop)
                break;

            for (uint16 arrayLayer = subResource.baseArrayLayer; arrayLayer < maxArrayLayers; arrayLayer++)
            {
                ImageSubResource currSubResource {};
                currSubResource.baseMipLevel = mipLevel;
                currSubResource.numLevels = 1;
                currSubResource.baseArrayLayer = arrayLayer;
                currSubResource.numLayers = 1;

                const uint64 subResourceKey = GetImageSubResourceKey(currSubResource);

                ResourceState foundResourceState;

                auto it = m_subResourceStates.Find(subResourceKey);

                if (it != m_subResourceStates.End())
                {
                    foundResourceState = it->second;
                }
                else
                {
                    foundResourceState = GetResourceState();
                }

                // needs to match expected state we're transitioning from. (currResourceState)
                if (firstSubResource)
                {
                    currResourceState = foundResourceState;
                    firstSubResource = false;
                }
                else if (foundResourceState != currResourceState)
                {
                    currResourceState = RS_UNDEFINED;
                    breakLoop = true;

                    break;
                }
            }
        }
    }

    if (hasStencil && currResourceState != currStencilState)
    {
        // Depth/stencil separate states sanity checks.
        if (currResourceState == newState)
        {
            Assert(onlyStencil);

            if (newState == RS_SHADER_RESOURCE)
            {
                Assert(currStencilState == RS_RENDER_TARGET);
            }
            else if (newState == RS_RENDER_TARGET)
            {
                Assert(currStencilState == RS_SHADER_RESOURCE);
            }
        }
        else if (currStencilState == newState)
        {
            Assert(onlyDepth);

            if (newState == RS_SHADER_RESOURCE)
            {
                Assert(currResourceState == RS_RENDER_TARGET);
            }
            else if (newState == RS_RENDER_TARGET)
            {
                Assert(currResourceState == RS_SHADER_RESOURCE);
            }
        }
    }

    VkImageAspectFlags aspectFlagBits = 0;

    if (isDepthStencil)
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (hasStencil)
        {
            aspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageSubresourceRange range {};
    range.aspectMask = aspectFlagBits;
    range.baseArrayLayer = MathUtil::Min(subResource.baseArrayLayer, NumArrayLayers() - 1);
    range.layerCount = MathUtil::Min(subResource.numLayers, NumArrayLayers() - range.baseArrayLayer);
    range.baseMipLevel = MathUtil::Min(subResource.baseMipLevel, NumMips() - 1);
    range.levelCount = MathUtil::Min(subResource.numLevels, NumMips() - range.baseMipLevel);

    VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

    if (hasStencil && currResourceState != currStencilState)
    {
        switch (currResourceState)
        {
        case RS_SHADER_RESOURCE:
            switch (currStencilState)
            {
            case RS_RENDER_TARGET:
                barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
                break;
            default:
                HYP_UNREACHABLE();
            }
            break;
        case RS_RENDER_TARGET:
            switch (currStencilState)
            {
            case RS_SHADER_RESOURCE:
                barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
                break;
            default:
                HYP_UNREACHABLE();
            }
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
    else
    {
        barrier.oldLayout = GetVkImageLayout(currResourceState, isDepthStencil && isAttachmentTexture, false, false);
    }

    if (onlyDepth && currStencilState == newState)
    {
        onlyDepth = false;
    }

    if (onlyStencil && currResourceState == newState)
    {
        onlyStencil = false;
    }

    barrier.newLayout = GetVkImageLayout(newState, isDepthStencil && isAttachmentTexture, onlyDepth, onlyStencil);
    barrier.srcAccessMask = GetVkAccessMask(currResourceState, isDepthStencil);
    barrier.dstAccessMask = GetVkAccessMask(newState, isDepthStencil);
    barrier.image = m_handle;
    barrier.subresourceRange = range;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        commandBuffer->GetVulkanHandle(),
        GetVkShaderStageMask(currResourceState, true, isDepthStencil, shaderModuleType),
        GetVkShaderStageMask(newState, false, isDepthStencil, shaderModuleType),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMips()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumArrayLayers())
    {
        if (onlyStencil)
        {
            m_stencilState = newState;
        }
        else
        {
            SetResourceState(newState);

            if (onlyDepth)
            {
                m_stencilState = currStencilState;
            }
        }

        return;
    }

    for (uint8 mipLevel = subResource.baseMipLevel; mipLevel < maxMipLevels; mipLevel++)
    {
        for (uint16 arrayLayer = subResource.baseArrayLayer; arrayLayer < maxArrayLayers; arrayLayer++)
        {
            ImageSubResource currSubResource {};
            currSubResource.baseMipLevel = uint8(mipLevel);
            currSubResource.numLevels = 1;
            currSubResource.baseArrayLayer = uint16(arrayLayer);
            currSubResource.numLayers = 1;

            const uint64 subResourceKey = GetImageSubResourceKey(currSubResource);

            auto it = m_subResourceStates.Empty() ? m_subResourceStates.End() : m_subResourceStates.Find(subResourceKey);

            if (it != m_subResourceStates.End())
            {
                it->second = newState;

                if (it->second == m_resourceState)
                {
                    // same state as overall image, remove from set
                    m_subResourceStates.Erase(it);
                }
            }
            else if (newState != m_resourceState)
            {
                m_subResourceStates.Set(subResourceKey, newState);
            }
        }
    }

    // No more remaining subresources, entire image was transitioned
    if (m_subResourceStates.Empty())
    {
        if (onlyStencil)
        {
            m_stencilState = newState;
        }
        else
        {
            SetResourceState(newState);

            if (onlyDepth)
            {
                m_stencilState = currStencilState;
            }
        }
    }
}

void VulkanGpuImage::Blit(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage)
{
    Blit(
        commandBuffer,
        srcImage,
        Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
        Rect<uint32> { 0, 0, m_textureDesc.extent.x, m_textureDesc.extent.y },
        ImageSubResource {
            .numLevels = srcImage->m_textureDesc.NumMips(),
            .numLayers = srcImage->m_textureDesc.NumArrayLayers()
        },
        ImageSubResource {
            .numLevels = m_textureDesc.NumMips(),
            .numLayers = m_textureDesc.NumArrayLayers()
    });
}

void VulkanGpuImage::Blit(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect)
{
    Blit(
        commandBuffer,
        srcImage,
        srcRect,
        dstRect,
        ImageSubResource {
            .numLevels = srcImage->m_textureDesc.NumMips(),
            .numLayers = srcImage->m_textureDesc.NumArrayLayers()
        },
        ImageSubResource {
            .numLevels = m_textureDesc.NumMips(),
            .numLayers = m_textureDesc.NumArrayLayers()
        });
}

void VulkanGpuImage::Blit(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource)
{
    const bool srcIsDepthStencil = srcImage->m_textureDesc.IsDepthStencil();
    const bool dstIsDepthStencil = m_textureDesc.IsDepthStencil();

    const bool srcIsAttachmentTexture = srcImage->m_textureDesc.imageUsage[IU_ATTACHMENT];
    const bool dstIsAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];

    VkImageAspectFlags srcAspectFlagBits = 0;

    if (srcIsDepthStencil)
    {
        srcAspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (TextureUtils::HasStencilComponent(srcImage->GetTextureFormat()))
        {
            srcAspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        srcAspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageAspectFlags dstAspectFlagBits = 0;

    if (dstIsDepthStencil)
    {
        dstAspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (TextureUtils::HasStencilComponent(GetTextureFormat()))
        {
            dstAspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        dstAspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // clamp src/dst rects to actual mip extents
    const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(srcSubResource.baseMipLevel);
    const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(dstSubResource.baseMipLevel);

    Rect<uint32> clampedSrcRect = srcRect;
    clampedSrcRect.x1 = MathUtil::Min(clampedSrcRect.x1, srcMipExtent.x);
    clampedSrcRect.y1 = MathUtil::Min(clampedSrcRect.y1, srcMipExtent.y);

    Rect<uint32> clampedDstRect = dstRect;
    clampedDstRect.x1 = MathUtil::Min(clampedDstRect.x1, dstMipExtent.x);
    clampedDstRect.y1 = MathUtil::Min(clampedDstRect.y1, dstMipExtent.y);

    // simple path; all resource states are same
    if (m_subResourceStates.Empty() && srcImage->m_subResourceStates.Empty())
    {
        const ResourceState srcResourceState = srcImage->m_resourceState;
        const ResourceState dstResourceState = m_resourceState;

        AssertDebug(srcResourceState == RS_COPY_SRC);
        AssertDebug(dstResourceState == RS_COPY_DST);

        VkImageBlit blit {
            .srcSubresource = {
                .aspectMask = srcAspectFlagBits,
                .mipLevel = srcSubResource.baseMipLevel,
                .baseArrayLayer = srcSubResource.baseArrayLayer,
                .layerCount = srcSubResource.numLayers },
            .srcOffsets = { { (int32_t)clampedSrcRect.x0, (int32_t)clampedSrcRect.y0, 0 }, { (int32_t)clampedSrcRect.x1, (int32_t)clampedSrcRect.y1, 1 } },
            .dstSubresource = { .aspectMask = dstAspectFlagBits, .mipLevel = dstSubResource.baseMipLevel, .baseArrayLayer = dstSubResource.baseArrayLayer, .layerCount = dstSubResource.numLayers },
            .dstOffsets = { { (int32_t)clampedDstRect.x0, (int32_t)clampedDstRect.y0, 0 }, { (int32_t)clampedDstRect.x1, (int32_t)clampedDstRect.y1, 1 } }
        };

        vkCmdBlitImage(
            commandBuffer->GetVulkanHandle(),
            srcImage->GetVulkanHandle(),
            GetVkImageLayout(srcResourceState, srcIsDepthStencil && srcIsAttachmentTexture, false, false),
            m_handle,
            GetVkImageLayout(dstResourceState, dstIsDepthStencil && dstIsAttachmentTexture, false, false),
            1, &blit,
            ToVkFilter(GetMinFilterMode()));

        return;
    }

    // complex path; per-subresource states
    for (uint16 layerIndex = 0; layerIndex < MathUtil::Min(srcSubResource.numLayers, srcImage->NumArrayLayers() - srcSubResource.baseArrayLayer); layerIndex++)
    {
        for (uint8 mipLevel = 0; mipLevel < MathUtil::Min(srcSubResource.numLevels, srcImage->NumMips() - srcSubResource.baseMipLevel); mipLevel++)
        {
            const ResourceState srcResourceState = srcImage->GetSubResourceState(ImageSubResource {
                .baseMipLevel = uint8(srcSubResource.baseMipLevel + mipLevel),
                .numLevels = 1,
                .baseArrayLayer = uint16(srcSubResource.baseArrayLayer + layerIndex),
                .numLayers = 1
            });

            const ResourceState dstResourceState = GetSubResourceState(ImageSubResource {
                .baseMipLevel = uint8(dstSubResource.baseMipLevel + mipLevel),
                .numLevels = 1,
                .baseArrayLayer = uint16(dstSubResource.baseArrayLayer + layerIndex),
                .numLayers = 1
            });

            AssertDebug(srcResourceState == RS_COPY_SRC);
            AssertDebug(dstResourceState == RS_COPY_DST);

            const Vec3u perMipSrcExtent = srcImage->GetTextureDesc().GetMipExtent(uint8(srcSubResource.baseMipLevel + mipLevel));
            const Vec3u perMipDstExtent = m_textureDesc.GetMipExtent(uint8(dstSubResource.baseMipLevel + mipLevel));

            Rect<uint32> perMipClampedSrcRect = srcRect;
            perMipClampedSrcRect.x1 = MathUtil::Min(perMipClampedSrcRect.x1, perMipSrcExtent.x);
            perMipClampedSrcRect.y1 = MathUtil::Min(perMipClampedSrcRect.y1, perMipSrcExtent.y);

            Rect<uint32> perMipClampedDstRect = dstRect;
            perMipClampedDstRect.x1 = MathUtil::Min(perMipClampedDstRect.x1, perMipDstExtent.x);
            perMipClampedDstRect.y1 = MathUtil::Min(perMipClampedDstRect.y1, perMipDstExtent.y);

            VkImageBlit blit {
                .srcSubresource = {
                    .aspectMask = srcAspectFlagBits,
                    .mipLevel = uint32(srcSubResource.baseMipLevel + mipLevel),
                    .baseArrayLayer = uint32(srcSubResource.baseArrayLayer + layerIndex),
                    .layerCount = 1 },
                .srcOffsets = { { (int32_t)perMipClampedSrcRect.x0, (int32_t)perMipClampedSrcRect.y0, 0 }, { (int32_t)perMipClampedSrcRect.x1, (int32_t)perMipClampedSrcRect.y1, 1 } },
                .dstSubresource = { .aspectMask = dstAspectFlagBits, .mipLevel = uint32(dstSubResource.baseMipLevel + mipLevel), .baseArrayLayer = uint32(dstSubResource.baseArrayLayer + layerIndex), .layerCount = 1 },
                .dstOffsets = { { (int32_t)perMipClampedDstRect.x0, (int32_t)perMipClampedDstRect.y0, 0 }, { (int32_t)perMipClampedDstRect.x1, (int32_t)perMipClampedDstRect.y1, 1 } }
            };

            vkCmdBlitImage(
                commandBuffer->GetVulkanHandle(),
                srcImage->GetVulkanHandle(),
                GetVkImageLayout(srcResourceState, srcIsDepthStencil && srcIsAttachmentTexture, false, false),
                m_handle,
                GetVkImageLayout(dstResourceState, dstIsDepthStencil && dstIsAttachmentTexture, false, false),
                1, &blit,
                ToVkFilter(GetMinFilterMode()));
        }
    }
}



void VulkanGpuImage::CopyFromBuffer(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuBuffer* srcBuffer,
    uint32 srcBufferOffset,
    uint8 dstMipIndex,
    uint16 dstArrayLayer) const
{
    VkImageAspectFlags aspectFlagBits = 0;

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];

    if (isDepthStencil)
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (TextureUtils::HasStencilComponent(m_textureDesc.format))
        {
            aspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // copy from staging to image
    const uint16 numArrayLayers = m_textureDesc.NumArrayLayers();
    AssertDebug(dstArrayLayer == UINT16_MAX || dstArrayLayer < numArrayLayers);

    const uint8 mipIdx = dstMipIndex != UINT8_MAX ? dstMipIndex : 0;

    const uint32 bufferOffsetStep = m_textureDesc.GetMipByteSize(mipIdx, /* includeArrayLayers */ false);
    const Vec3u mipExtent = m_textureDesc.GetMipExtent(mipIdx);

    if (dstArrayLayer == UINT16_MAX)
    {
        for (uint16 arrayLayerIdx = 0; arrayLayerIdx < numArrayLayers; arrayLayerIdx++)
        {
            VkBufferImageCopy region {};
            region.bufferOffset = srcBufferOffset + (bufferOffsetStep * arrayLayerIdx);
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = aspectFlagBits;
            region.imageSubresource.mipLevel = mipIdx;
            region.imageSubresource.baseArrayLayer = arrayLayerIdx;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = VkExtent3D { mipExtent.x, mipExtent.y, mipExtent.z };

            // https://docs.vulkan.org/spec/latest/chapters/copies.html#VUID-vkCmdCopyBufferToImage-dstImage-07975
            // bufferOffset must be a multiple texel block size
            AssertDebug(region.bufferOffset % (TextureUtils::BytesPerComponent(m_textureDesc.format) * TextureUtils::NumComponents(m_textureDesc.format)) == 0);

            vkCmdCopyBufferToImage(
                commandBuffer->GetVulkanHandle(),
                srcBuffer->GetVulkanHandle(),
                m_handle,
                GetVkImageLayout(m_resourceState, isDepthStencil && isAttachmentTexture),
                1,
                &region);
        }
    }
    else
    {
        VkBufferImageCopy region {};
        region.bufferOffset = srcBufferOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspectFlagBits;
        region.imageSubresource.mipLevel = mipIdx;
        region.imageSubresource.baseArrayLayer = dstArrayLayer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { mipExtent.x, mipExtent.y, mipExtent.z };

        // https://docs.vulkan.org/spec/latest/chapters/copies.html#VUID-vkCmdCopyBufferToImage-dstImage-07975
        // bufferOffset must be a multiple texel block size
        AssertDebug(region.bufferOffset % (TextureUtils::BytesPerComponent(m_textureDesc.format) * TextureUtils::NumComponents(m_textureDesc.format)) == 0);

        vkCmdCopyBufferToImage(
            commandBuffer->GetVulkanHandle(),
            srcBuffer->GetVulkanHandle(),
            m_handle,
            GetVkImageLayout(m_resourceState, isDepthStencil && isAttachmentTexture),
            1,
            &region);
    }
}

void VulkanGpuImage::CopyToBuffer(
    VulkanCommandBuffer* commandBuffer,
    VulkanGpuBuffer* dstBuffer,
    const ImageSubResource& subResource) const
{
    Assert(dstBuffer != nullptr && dstBuffer->IsCreated(), "Destination buffer is null or invalid !");
    Assert(dstBuffer->Size() >= m_size, "Destination buffer is too small to hold image data!");

    ImageSubResource newSubResource = subResource;
    newSubResource.numLayers = MathUtil::Min(subResource.numLayers, NumArrayLayers() - subResource.baseArrayLayer);
    newSubResource.numLevels = MathUtil::Min(subResource.numLevels, NumMips() - subResource.baseMipLevel);

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];

    VkImageAspectFlags aspectFlagBits = 0;

    if (isDepthStencil)
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (TextureUtils::HasStencilComponent(m_textureDesc.format))
        {
            aspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    for (uint8 mipIndex = newSubResource.baseMipLevel; mipIndex < newSubResource.baseMipLevel + newSubResource.numLevels; mipIndex++)
    {
        // copy from staging to image
        const Vec3u mipExtent = m_textureDesc.GetMipExtent(mipIndex);

        const size_t layerStep = m_textureDesc.GetMipByteSize(mipIndex, /* includeArrayLayers */ false);

        for (uint16 layerIndex = newSubResource.baseArrayLayer; layerIndex < newSubResource.baseArrayLayer + newSubResource.numLayers; layerIndex++)
        {
            ImageSubResource currSubResource;
            currSubResource.baseMipLevel = mipIndex;
            currSubResource.numLevels = 1;
            currSubResource.baseArrayLayer = layerIndex;
            currSubResource.numLayers = 1;

            VkBufferImageCopy region {};
            region.bufferOffset = bufferOffset + ((layerIndex - newSubResource.baseArrayLayer) * layerStep);
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = aspectFlagBits;
            region.imageSubresource.mipLevel = mipIndex;
            region.imageSubresource.baseArrayLayer = layerIndex;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = VkExtent3D { mipExtent.x, mipExtent.y, mipExtent.z };

            vkCmdCopyImageToBuffer(
                commandBuffer->GetVulkanHandle(),
                m_handle,
                GetVkImageLayout(GetSubResourceState(currSubResource), isDepthStencil && isAttachmentTexture),
                dstBuffer->GetVulkanHandle(),
                1,
                &region);
        }

        bufferOffset += layerStep * newSubResource.numLayers;
    }

    dstBuffer->InsertHostReadBarrier(commandBuffer);
}

void VulkanGpuImage::Fill(
    VulkanCommandBuffer* commandBuffer,
    float value,
    const ImageSubResource& subResource,
    const Vec3u& offset,
    const Vec3u& extent)
{
    const bool isDepthStencil = m_textureDesc.IsDepthStencil();

    VkImageAspectFlags aspectFlagBits = 0;

    if (isDepthStencil)
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (TextureUtils::HasStencilComponent(m_textureDesc.format))
        {
            aspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        aspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageSubresourceRange subresourceRange {};
    subresourceRange.aspectMask = aspectFlagBits;
    subresourceRange.baseMipLevel = subResource.baseMipLevel;
    subresourceRange.levelCount = subResource.numLevels != UINT8_MAX ? subResource.numLevels : 1;
    subresourceRange.baseArrayLayer = subResource.baseArrayLayer;
    subresourceRange.layerCount = subResource.numLayers != UINT16_MAX ? subResource.numLayers : 1;

    InsertBarrier(commandBuffer, subResource, RS_COPY_DST, ShaderModuleType::None);

    if (isDepthStencil)
    {
        VkClearDepthStencilValue clearValue = { .depth = value, .stencil = 0 };

        vkCmdClearDepthStencilImage(
            commandBuffer->GetVulkanHandle(),
            m_handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearValue,
            1,
            &subresourceRange);
    }
    else
    {
        VkClearColorValue clearValue = { .float32 = { value, value, value, value } };

        vkCmdClearColorImage(
            commandBuffer->GetVulkanHandle(),
            m_handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearValue,
            1,
            &subresourceRange);
    }
}

void VulkanGpuImage::CopyFrom(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage,
    const Vec3u& srcOffset,
    const Vec3u& dstOffset,
    const Vec3u& extent,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource)
{
    const bool srcIsDepthStencil = srcImage->GetTextureDesc().IsDepthStencil();
    const bool dstIsDepthStencil = m_textureDesc.IsDepthStencil();

    const bool srcIsAttachmentTexture = srcImage->GetTextureDesc().imageUsage[IU_ATTACHMENT];
    const bool dstIsAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];

    VkImageAspectFlags srcAspectFlagBits = 0;

    if (srcIsDepthStencil)
    {
        srcAspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (TextureUtils::HasStencilComponent(srcImage->GetTextureFormat()))
        {
            srcAspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        srcAspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageAspectFlags dstAspectFlagBits = 0;

    if (dstIsDepthStencil)
    {
        dstAspectFlagBits |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (TextureUtils::HasStencilComponent(GetTextureFormat()))
        {
            dstAspectFlagBits |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        dstAspectFlagBits |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // normalize counts
    ImageSubResource newSrcSubResource = srcSubResource;
    newSrcSubResource.numLayers = MathUtil::Min(srcSubResource.numLayers, srcImage->NumArrayLayers() - srcSubResource.baseArrayLayer);
    newSrcSubResource.numLevels = MathUtil::Min(srcSubResource.numLevels, srcImage->NumMips() - srcSubResource.baseMipLevel);

    ImageSubResource newDstSubResource = dstSubResource;
    newDstSubResource.numLayers = MathUtil::Min(dstSubResource.numLayers, NumArrayLayers() - dstSubResource.baseArrayLayer);
    newDstSubResource.numLevels = MathUtil::Min(dstSubResource.numLevels, NumMips() - dstSubResource.baseMipLevel);

    // clamp extent to min of src and dst mip dimensions
    const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(newSrcSubResource.baseMipLevel);
    const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(newDstSubResource.baseMipLevel);

    Vec3u clampedExtent = extent;
    clampedExtent.x = MathUtil::Min(clampedExtent.x, srcMipExtent.x - srcOffset.x, dstMipExtent.x - dstOffset.x);
    clampedExtent.y = MathUtil::Min(clampedExtent.y, srcMipExtent.y - srcOffset.y, dstMipExtent.y - dstOffset.y);
    clampedExtent.z = MathUtil::Min(clampedExtent.z, srcMipExtent.z - srcOffset.z, dstMipExtent.z - dstOffset.z);

    // simple path; all resource states are same
    if (m_subResourceStates.Empty() && srcImage->m_subResourceStates.Empty())
    {
        const ResourceState srcResourceState = srcImage->m_resourceState;
        const ResourceState dstResourceState = m_resourceState;

        AssertDebug(srcResourceState == RS_COPY_SRC);
        AssertDebug(dstResourceState == RS_COPY_DST);

        VkImageCopy copy {};
        copy.extent = { clampedExtent.x, clampedExtent.y, clampedExtent.z };
        copy.srcOffset = { int(srcOffset.x), int(srcOffset.y), int(srcOffset.z) };
        copy.dstOffset = { int(dstOffset.x), int(dstOffset.y), int(dstOffset.z) };
        copy.srcSubresource = {
            .aspectMask = srcAspectFlagBits,
            .mipLevel = newSrcSubResource.baseMipLevel,
            .baseArrayLayer = newSrcSubResource.baseArrayLayer,
            .layerCount = newSrcSubResource.numLayers
        };
        copy.dstSubresource = {
            .aspectMask = dstAspectFlagBits,
            .mipLevel = newDstSubResource.baseMipLevel,
            .baseArrayLayer = newDstSubResource.baseArrayLayer,
            .layerCount = newDstSubResource.numLayers
        };

        vkCmdCopyImage(
            commandBuffer->GetVulkanHandle(),
            srcImage->GetVulkanHandle(),
            GetVkImageLayout(srcResourceState, srcIsDepthStencil && srcIsAttachmentTexture),
            m_handle,
            GetVkImageLayout(dstResourceState, dstIsDepthStencil && dstIsAttachmentTexture),
            1, &copy);

        return;
    }

    // complex path; per-subresource states
    for (uint16 layerIndex = 0; layerIndex < MathUtil::Min(newSrcSubResource.numLayers, srcImage->NumArrayLayers() - newSrcSubResource.baseArrayLayer); layerIndex++)
    {
        for (uint8 mipLevel = 0; mipLevel < MathUtil::Min(newSrcSubResource.numLevels, srcImage->NumMips() - newSrcSubResource.baseMipLevel); mipLevel++)
        {
            const ResourceState srcResourceState = srcImage->GetSubResourceState(ImageSubResource {
                .baseMipLevel = uint8(newSrcSubResource.baseMipLevel + mipLevel),
                .numLevels = 1,
                .baseArrayLayer = uint16(newSrcSubResource.baseArrayLayer + layerIndex),
                .numLayers = 1
            });

            const ResourceState dstResourceState = GetSubResourceState(ImageSubResource {
                .baseMipLevel = uint8(newDstSubResource.baseMipLevel + mipLevel),
                .numLevels = 1,
                .baseArrayLayer = uint16(newDstSubResource.baseArrayLayer + layerIndex),
                .numLayers = 1
            });

           // AssertDebug(srcResourceState == RS_COPY_SRC);
           // AssertDebug(dstResourceState == RS_COPY_DST);

            const Vec3u perMipSrcExtent = srcImage->GetTextureDesc().GetMipExtent(uint8(newSrcSubResource.baseMipLevel + mipLevel));
            const Vec3u perMipDstExtent = m_textureDesc.GetMipExtent(uint8(newDstSubResource.baseMipLevel + mipLevel));

            Vec3u perMipClampedExtent = extent;
            perMipClampedExtent.x = MathUtil::Min(perMipClampedExtent.x, perMipSrcExtent.x - srcOffset.x, perMipDstExtent.x - dstOffset.x);
            perMipClampedExtent.y = MathUtil::Min(perMipClampedExtent.y, perMipSrcExtent.y - srcOffset.y, perMipDstExtent.y - dstOffset.y);
            perMipClampedExtent.z = MathUtil::Min(perMipClampedExtent.z, perMipSrcExtent.z - srcOffset.z, perMipDstExtent.z - dstOffset.z);

            VkImageCopy copy {};
            copy.extent = { perMipClampedExtent.x, perMipClampedExtent.y, perMipClampedExtent.z };
            copy.srcOffset = { int(srcOffset.x), int(srcOffset.y), int(srcOffset.z) };
            copy.dstOffset = { int(dstOffset.x), int(dstOffset.y), int(dstOffset.z) };
            copy.srcSubresource = {
                .aspectMask = srcAspectFlagBits,
                .mipLevel = uint32(newSrcSubResource.baseMipLevel + mipLevel),
                .baseArrayLayer = uint32(newSrcSubResource.baseArrayLayer + layerIndex),
                .layerCount = 1
            };
            copy.dstSubresource = {
                .aspectMask = dstAspectFlagBits,
                .mipLevel = uint32(newDstSubResource.baseMipLevel + mipLevel),
                .baseArrayLayer = uint32(newDstSubResource.baseArrayLayer + layerIndex),
                .layerCount = 1
            };

            vkCmdCopyImage(
                commandBuffer->GetVulkanHandle(),
                srcImage->GetVulkanHandle(),
                GetVkImageLayout(srcResourceState, srcIsDepthStencil && srcIsAttachmentTexture),
                m_handle,
                GetVkImageLayout(dstResourceState, dstIsDepthStencil && dstIsAttachmentTexture),
                1, &copy);
        }
    }
}

VulkanGpuImageViewRef VulkanGpuImage::MakeLayerImageView(uint32 layerIndex) const
{
    if (m_handle == VK_NULL_HANDLE)
    {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to create image view on uninitialized image");

        return VulkanGpuImageViewRef::Null();
    }

    return RI.MakeImageView(
        MakeStrongRef(this),
        0,
        m_textureDesc.NumMips(),
        layerIndex,
        1);
}

#ifdef HYP_RHI_DEBUG_NAMES
void VulkanGpuImage::SetDebugName(Name name)
{
    GpuImageBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    if (m_allocation != VK_NULL_HANDLE)
    {
        vmaSetAllocationName(RI.GetDevice()->GetVmaAllocator(), m_allocation, strName);
    }

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
        objectNameInfo.objectHandle = (uint64)m_handle;
        objectNameInfo.pObjectName = strName;

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}
#endif

#pragma endregion VulkanGpuImage

} // namespace Hyperion
