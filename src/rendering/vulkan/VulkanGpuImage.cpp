/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/RenderQueue.hpp>

#include <util/img/ImageUtil.hpp>

#include <core/utilities/Pair.hpp>

#include <core/functional/Proc.hpp>

#include <core/debug/Debug.hpp>

#include <vulkan/vulkan.h>

#include <VulkanGpuImage.generated.inl>

namespace Hyperion {

static constexpr SizeType MaxImageBytes = 1024 * 1024 * 1024; // 1 GiB

extern VulkanRenderBackend* g_renderBackend;

extern VkImageLayout GetVkImageLayout(ResourceState);
extern VkAccessFlags GetVkAccessMask(ResourceState);
extern VkPipelineStageFlags GetVkShaderStageMask(ResourceState, bool, ShaderModuleType);

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
            HYP_GFX_ASSERT(m_isHandleOwned, "If allocation is not VK_NULL_HANDLE, is_handle_owned should be true");

            vmaDestroyImage(g_renderBackend->GetDevice()->GetAllocator(), m_handle, m_allocation);
            m_allocation = VK_NULL_HANDLE;
        }

        m_handle = VK_NULL_HANDLE;

        // reset back to default
        m_isHandleOwned = true;

        m_resourceState = RS_UNDEFINED;
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

    const uint32 numLayers = NumArrayLayers();
    const uint32 numMipmaps = NumMips();

    for (uint32 face = 0; face < numLayers; face++)
    {
        for (int32 i = 1; i < int32(numMipmaps + 1); i++)
        {
            const int mipWidth = int(helpers::MipmapSize(m_textureDesc.extent.x, i)),
                      mipHeight = int(helpers::MipmapSize(m_textureDesc.extent.y, i)),
                      mipDepth = int(helpers::MipmapSize(m_textureDesc.extent.z, i));

            /* Memory barrier for transfer - note that after generating the mipmaps,
                we'll still need to transfer into a layout primed for reading from shaders. */

            const ImageSubResource src {
                .flags = m_textureDesc.IsDepthStencil()
                    ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                    : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
                .baseArrayLayer = face,
                .baseMipLevel = uint32(i - 1)
            };

            const ImageSubResource dst {
                .flags = src.flags,
                .baseArrayLayer = src.baseArrayLayer,
                .baseMipLevel = uint32(i)
            };

            InsertBarrier(
                commandBuffer,
                src,
                RS_COPY_SRC,
                SMT_UNSET);

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

            const VkImageAspectFlags aspectFlagBits =
                (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
                | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
                | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

            /* Blit src -> dst */
            const VkImageBlit blit {
                .srcSubresource = {
                    .aspectMask = aspectFlagBits,
                    .mipLevel = src.baseMipLevel,
                    .baseArrayLayer = src.baseArrayLayer,
                    .layerCount = src.numLayers },
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

    VkImageLayout initialLayout = GetVkImageLayout(initialState);

    if (!m_isHandleOwned)
    {
        HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE, "If m_isHandleOwned is false, the image handle must not be VK_NULL_HANDLE.");

        return {};
    }

    if (GetByteSize() > MaxImageBytes)
    {
        return HYP_MAKE_ERROR(RendererError, "Image size exceeds maximum supported size of %llu bytes", MaxImageBytes);
    }

    const Vec3u extent = GetExtent();

    const TextureFormat format = GetTextureFormat();
    const TextureType type = GetType();

    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];
    const bool isRwTexture = m_textureDesc.imageUsage[IU_STORAGE];
    const bool isExternalMemory = m_textureDesc.imageUsage[IU_EXTERNAL];

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool isBlended = m_textureDesc.IsBlended();
    const bool isSrgb = m_textureDesc.IsSrgb();

    const bool hasMipmaps = m_textureDesc.HasMipMaps();
    const uint32 numMipmaps = m_textureDesc.NumMips();
    const uint32 numLayers = m_textureDesc.NumArrayLayers();

    if (extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

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

    if (isRwTexture)
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
        HYP_LOG(RenderingBackend, Debug, "Image requires blending, enabling format flag...");

        vkFormatFeatures |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (m_textureDesc.IsTextureCube() || m_textureDesc.IsTextureCubeArray())
    {
        HYP_LOG(RenderingBackend, Debug, "Creating cubemap, enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.");

        vkImageCreateFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    RendererResult formatSupportResult = g_renderBackend->GetDevice()->GetFeatures().GetImageFormatProperties(
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

    const QueueFamilyIndices& qfIndices = g_renderBackend->GetDevice()->GetQueueFamilyIndices();
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
            g_renderBackend->GetDevice()->GetAllocator(),
            &imageInfo, &allocInfo, &memTypeIndex);

        VULKAN_CHECK(res);

        VmaPoolCreateInfo poolCreateInfo {};
        poolCreateInfo.memoryTypeIndex = memTypeIndex;
        poolCreateInfo.pMemoryAllocateNext = (void*)&ExportAllocInfo;

        VmaPool pool;
        res = vmaCreatePool(g_renderBackend->GetDevice()->GetAllocator(), &poolCreateInfo, &pool);
        VULKAN_CHECK(res); //// \todo Have to destroy this pool later!

        allocInfo.pool = pool;

        externalMemoryImageCreateInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR };
        externalMemoryImageCreateInfo.handleTypes = ExportAllocInfo.handleTypes;

        imageInfo.pNext = &externalMemoryImageCreateInfo;
    }

    VULKAN_CHECK_MSG(
        vmaCreateImage(
            g_renderBackend->GetDevice()->GetAllocator(),
            &imageInfo,
            &allocInfo,
            &m_handle,
            &m_allocation,
            nullptr),
        "Failed to create gpu image!");

#ifdef HYP_DEBUG_MODE
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
            vmaDestroyImage(g_renderBackend->GetDevice()->GetAllocator(), m_handle, m_allocation);
            m_allocation = VK_NULL_HANDLE;
        }

        m_handle = VK_NULL_HANDLE;
        m_resourceState = RS_UNDEFINED;
        m_subResourceStates.Clear();

        CheckResultOrReturn(Create());

        if (previousResourceState != RS_UNDEFINED)
        {
            SetResourceState(RS_UNDEFINED);

            VulkanFrame* frame = g_renderBackend->GetCurrentFrame();
            RenderQueue& renderQueue = frame->renderQueue;
            renderQueue << ::Hyperion::InsertBarrier(this, previousResourceState);
        }
    }

    return {};
}

auto VulkanGpuImage::GetNativeHandle() const -> HANDLE
{
    Assert(IsCreated());
    Assert(m_textureDesc.imageUsage & IU_EXTERNAL);

#ifdef HYP_WINDOWS
    VkMemoryGetWin32HandleInfoKHR getHandleInfo { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
    getHandleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;

    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(g_renderBackend->GetDevice()->GetAllocator(), m_allocation, &allocInfo);
    getHandleInfo.memory = allocInfo.deviceMemory;

    HANDLE handle;

    VkResult res = g_vulkanDynamicFunctions->vkGetMemoryWin32HandleKHR(
        g_renderBackend->GetDevice()->GetDevice(),
        &getHandleInfo,
        &handle);

    Assert(res == VK_SUCCESS, "Failed to get external memory handle for image! VkResult: %d", res);

    return handle;
#else
    VkMemoryGetFdInfoKHR getFdInfo { VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR };
    getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(g_renderBackend->GetDevice()->GetAllocator(), m_allocation, &allocInfo);
    getFdInfo.memory = allocInfo.deviceMemory;

    int fd;

    VkResult res = g_vulkanDynamicFunctions->vkGetMemoryFdKHR(
        g_renderBackend->GetDevice()->GetDevice(),
        &getFdInfo,
        &fd);

    Assert(res == VK_SUCCESS, "Failed to get external memory fd for image! VkResult: %d", res);

    return reinterpret_cast<HANDLE>(fd);
#endif
}

void VulkanGpuImage::SetResourceState(ResourceState newState)
{
    m_resourceState = newState;

    m_subResourceStates.Clear();
}

ResourceState VulkanGpuImage::GetSubResourceState(const ImageSubResource& subResource) const
{
    auto it = m_subResourceStates.Find(subResource.GetSubResourceKey());

    if (it == m_subResourceStates.End())
    {
        return m_resourceState;
    }

    return it->second;
}

void VulkanGpuImage::SetSubResourceState(const ImageSubResource& subResource, ResourceState newState)
{
    m_subResourceStates.Set(subResource.GetSubResourceKey(), newState);
}

void VulkanGpuImage::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    ResourceState newState,
    ShaderModuleType shaderModuleType)
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_NONE;

    if (m_textureDesc.IsDepthStencil())
    {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL;
    }
    else
    {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    }

    InsertBarrier(
        commandBuffer,
        ImageSubResource {
            .flags = flags,
            .numLayers = ~0u,
            .numLevels = ~0u },
        newState,
        shaderModuleType);
}

void VulkanGpuImage::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    const ImageSubResource& subResource,
    ResourceState newState,
    ShaderModuleType shaderModuleType)
{
    if (m_handle == VK_NULL_HANDLE)
    {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to insert a resource barrier but image was not defined");

        return;
    }

    const ResourceState prevResourceState = GetSubResourceState(subResource);

#ifdef HYP_DEBUG_MODE
    for (int mipLevel = int(subResource.baseMipLevel); mipLevel < int(subResource.baseMipLevel) + int(MathUtil::Min(subResource.numLevels, NumMips())); mipLevel++)
    {
        for (int arrayLayer = int(subResource.baseArrayLayer); arrayLayer < int(subResource.baseArrayLayer) + int(MathUtil::Min(subResource.numLayers, NumLayers())); arrayLayer++)
        {
            const uint64 subResourceKey = GetImageSubResourceKey(arrayLayer, mipLevel);

            auto it = m_subResourceStates.Find(subResourceKey);

            if (it != m_subResourceStates.End())
            {
                HYP_GFX_ASSERT(
                    it->second == prevResourceState,
                    "Sub resource state mismatch for image: mip %d, layer %d",
                    mipLevel,
                    arrayLayer);
            }
        }
    }
#endif

    const VkImageAspectFlags aspectFlagBits =
        (subResource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (subResource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (subResource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range {};
    range.aspectMask = aspectFlagBits;
    range.baseArrayLayer = subResource.baseArrayLayer;
    range.layerCount = subResource.numLayers;
    range.baseMipLevel = subResource.baseMipLevel;
    range.levelCount = subResource.numLevels;

    VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = GetVkImageLayout(prevResourceState);
    barrier.newLayout = GetVkImageLayout(newState);
    barrier.srcAccessMask = GetVkAccessMask(prevResourceState);
    barrier.dstAccessMask = GetVkAccessMask(newState);
    barrier.image = m_handle;
    barrier.subresourceRange = range;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        commandBuffer->GetVulkanHandle(),
        GetVkShaderStageMask(prevResourceState, true, shaderModuleType),
        GetVkShaderStageMask(newState, false, shaderModuleType),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    if (newState == m_resourceState)
    {
        for (int mipLevel = int(subResource.baseMipLevel); mipLevel < int(subResource.baseMipLevel) + int(MathUtil::Min(subResource.numLevels, NumMips())); mipLevel++)
        {
            for (int arrayLayer = int(subResource.baseArrayLayer); arrayLayer < int(subResource.baseArrayLayer) + int(MathUtil::Min(subResource.numLayers, NumLayers())); arrayLayer++)
            {
                const uint64 subResourceKey = GetImageSubResourceKey(arrayLayer, mipLevel);

                m_subResourceStates.Erase(subResourceKey);
            }
        }

        return;
    }
    else if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMips()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumLayers())
    {
        // If all subresources will be set, just set the whole resource state
        SetResourceState(newState);

        return;
    }

    for (int mipLevel = int(subResource.baseMipLevel); mipLevel < int(subResource.baseMipLevel) + int(MathUtil::Min(subResource.numLevels, NumMips())); mipLevel++)
    {
        for (int arrayLayer = int(subResource.baseArrayLayer); arrayLayer < int(subResource.baseArrayLayer) + int(MathUtil::Min(subResource.numLayers, NumLayers())); arrayLayer++)
        {
            const uint64 subResourceKey = GetImageSubResourceKey(arrayLayer, mipLevel);

            m_subResourceStates.Set(subResourceKey, newState);
        }
    }
}

RendererResult VulkanGpuImage::Blit(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage)
{
    return Blit(
        commandBuffer,
        srcImage,
        Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
        Rect<uint32> { 0, 0, m_textureDesc.extent.x, m_textureDesc.extent.y });
}

RendererResult VulkanGpuImage::Blit(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage,
    uint32 srcMip,
    uint32 dstMip,
    uint32 srcFace,
    uint32 dstFace)
{
    return Blit(
        commandBuffer,
        srcImage,
        Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
        Rect<uint32> { 0, 0, m_textureDesc.extent.x, m_textureDesc.extent.y },
        srcMip, dstMip, srcFace, dstFace);
}

RendererResult VulkanGpuImage::Blit(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect)
{
    const uint32 numLayers = MathUtil::Min(NumArrayLayers(), srcImage->NumArrayLayers());

    for (uint32 face = 0; face < numLayers; face++)
    {
        const ImageSubResource src {
            .flags = srcImage->GetTextureDesc().IsDepthStencil() ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .baseArrayLayer = face,
            .baseMipLevel = 0
        };

        const ImageSubResource dst {
            .flags = m_textureDesc.IsDepthStencil() ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .baseArrayLayer = face,
            .baseMipLevel = 0
        };

        const ResourceState srcResourceState = static_cast<const VulkanGpuImage*>(srcImage)->GetSubResourceState(src);
        const ResourceState dstResourceState = GetSubResourceState(dst);

        const VkImageAspectFlags aspectFlagBits =
            (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

        VkImageBlit blit {
            .srcSubresource = {
                .aspectMask = aspectFlagBits,
                .mipLevel = src.baseMipLevel,
                .baseArrayLayer = src.baseArrayLayer,
                .layerCount = src.numLayers },
            .srcOffsets = { { (int32_t)srcRect.x0, (int32_t)srcRect.y0, 0 }, { (int32_t)srcRect.x1, (int32_t)srcRect.y1, 1 } },
            .dstSubresource = { .aspectMask = aspectFlagBits, .mipLevel = dst.baseMipLevel, .baseArrayLayer = dst.baseArrayLayer, .layerCount = dst.numLayers },
            .dstOffsets = { { (int32_t)dstRect.x0, (int32_t)dstRect.y0, 0 }, { (int32_t)dstRect.x1, (int32_t)dstRect.y1, 1 } }
        };

        vkCmdBlitImage(
            commandBuffer->GetVulkanHandle(),
            srcImage->GetVulkanHandle(),
            GetVkImageLayout(srcResourceState),
            m_handle,
            GetVkImageLayout(dstResourceState),
            1, &blit,
            ToVkFilter(GetMinFilterMode()));
    }

    return {};
}

RendererResult VulkanGpuImage::Blit(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuImage* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect,
    uint32 srcMip,
    uint32 dstMip,
    uint32 srcFace,
    uint32 dstFace)
{
    const uint32 numLayers = MathUtil::Min(NumArrayLayers(), srcImage->NumArrayLayers());

    const ImageSubResource src {
        .flags = srcImage->GetTextureDesc().IsDepthStencil() ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
        .baseArrayLayer = srcFace,
        .baseMipLevel = srcMip
    };

    const ImageSubResource dst {
        .flags = m_textureDesc.IsDepthStencil() ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
        .baseArrayLayer = dstFace,
        .baseMipLevel = dstMip
    };

    const ResourceState srcResourceState = static_cast<const VulkanGpuImage*>(srcImage)->GetSubResourceState(src);
    const ResourceState dstResourceState = GetSubResourceState(dst);

    const VkImageAspectFlags aspectFlagBits =
        (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
        | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageBlit blit {
        .srcSubresource = {
            .aspectMask = aspectFlagBits,
            .mipLevel = srcMip,
            .baseArrayLayer = src.baseArrayLayer,
            .layerCount = 1 },
        .srcOffsets = { { int32(srcRect.x0), int32(srcRect.y0), 0 }, { int32(srcRect.x1), int32(srcRect.y1), 1 } },
        .dstSubresource = { .aspectMask = aspectFlagBits, .mipLevel = dstMip, .baseArrayLayer = dst.baseArrayLayer, .layerCount = 1 },
        .dstOffsets = { { int32(dstRect.x0), int32(dstRect.y0), 0 }, { int32(dstRect.x1), int32(dstRect.y1), 1 } }
    };

    vkCmdBlitImage(
        commandBuffer->GetVulkanHandle(),
        srcImage->GetVulkanHandle(),
        GetVkImageLayout(srcResourceState),
        m_handle,
        GetVkImageLayout(dstResourceState),
        1, &blit,
        ToVkFilter(GetMinFilterMode()));

    return {};
}

void VulkanGpuImage::CopyFromBuffer(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuBuffer* srcBuffer,
    uint32 srcBufferOffset,
    uint8 dstMipIndex,
    uint16 dstArrayLayer) const
{
    const auto flags = m_textureDesc.IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspectFlagBits =
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    // copy from staging to image
    const uint16 numArrayLayers = m_textureDesc.NumArrayLayers();
    AssertDebug(dstArrayLayer == UINT16_MAX || dstArrayLayer < numArrayLayers);

    const uint8 mipIdx = dstMipIndex != UINT8_MAX ? dstMipIndex : 0;

    const uint32 bufferOffsetStep = m_textureDesc.GetMipByteSize(mipIdx) / numArrayLayers;
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

            vkCmdCopyBufferToImage(
                commandBuffer->GetVulkanHandle(),
                srcBuffer->GetVulkanHandle(),
                m_handle,
                GetVkImageLayout(m_resourceState),
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

        vkCmdCopyBufferToImage(
            commandBuffer->GetVulkanHandle(),
            srcBuffer->GetVulkanHandle(),
            m_handle,
            GetVkImageLayout(m_resourceState),
            1,
            &region);
    }
}

void VulkanGpuImage::CopyToBuffer(VulkanCommandBuffer* commandBuffer, VulkanGpuBuffer* dstBuffer) const
{
    HYP_GFX_ASSERT(dstBuffer != nullptr && dstBuffer->IsCreated(), "Destination buffer is null or invalid !");
    HYP_GFX_ASSERT(dstBuffer->Size() >= m_size, "Destination buffer is too small to hold image data!");

    const auto flags = m_textureDesc.IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspectFlagBits =
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    // copy from staging to image
    const uint32 numLayers = NumArrayLayers();
    const uint32 bufferOffsetStep = uint32(m_size) / numLayers;

    for (uint32 layerIndex = 0; layerIndex < numLayers; layerIndex++)
    {
        VkBufferImageCopy region {};
        region.bufferOffset = layerIndex * bufferOffsetStep;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspectFlagBits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = layerIndex;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_textureDesc.extent.x, m_textureDesc.extent.y, m_textureDesc.extent.z };

        vkCmdCopyImageToBuffer(
            commandBuffer->GetVulkanHandle(),
            m_handle,
            GetVkImageLayout(m_resourceState),
            dstBuffer->GetVulkanHandle(),
            1,
            &region);
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

        return VulkanGpuImageViewRef();
    }

    return g_renderBackend->MakeImageView(
        HandleFromThis(),
        0,
        NumMips(),
        layerIndex,
        1);
}

#ifdef HYP_DEBUG_MODE

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
        vmaSetAllocationName(g_renderBackend->GetDevice()->GetAllocator(), m_allocation, strName);
    }

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(g_renderBackend->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanGpuImage

} // namespace Hyperion
