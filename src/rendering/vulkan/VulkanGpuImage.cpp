/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

#include <core/containers/Array.hpp>

#include <core/utilities/Pair.hpp>

#include <core/functional/Proc.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

extern VkImageLayout GetVkImageLayout(ResourceState);
extern VkAccessFlags GetVkAccessMask(ResourceState);
extern VkPipelineStageFlags GetVkShaderStageMask(ResourceState, bool, ShaderModuleType);

#pragma endregion VulkanImagePlatformImpl

#pragma region VulkanGpuImage

VulkanGpuImage::VulkanGpuImage(const TextureDesc& textureDesc)
    : GpuImageBase(textureDesc)
{
    m_size = textureDesc.GetByteSize();
}

VulkanGpuImage::~VulkanGpuImage()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE);
}

bool VulkanGpuImage::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

bool VulkanGpuImage::IsOwned() const
{
    return m_isHandleOwned;
}

RendererResult VulkanGpuImage::GenerateMipmaps(CommandBufferBase* commandBuffer)
{
    if (m_handle == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot generate mipmaps on uninitialized image");
    }

    const uint32 numFaces = NumFaces();
    const uint32 numMipmaps = NumMipmaps();

    for (uint32 face = 0; face < numFaces; face++)
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
                if (face == numFaces - 1)
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
                VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
                m_handle,
                GetVkImageLayout(RS_COPY_SRC),
                m_handle,
                GetVkImageLayout(RS_COPY_DST),
                1, &blit,
                m_textureDesc.IsDepthStencil() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR // TODO: base on filter mode
            );
        }
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanGpuImage::Create()
{
    if (IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    return Create(RS_UNDEFINED);
}

RendererResult VulkanGpuImage::Create(ResourceState initialState)
{
    if (IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    VkImageLayout initialLayout = GetVkImageLayout(initialState);

    if (!m_isHandleOwned)
    {
        HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE, "If is_handle_owned is set to false, the image handle must not be VK_NULL_HANDLE.");

        HYPERION_RETURN_OK;
    }

    const Vec3u extent = GetExtent();
    const TextureFormat format = GetTextureFormat();
    const TextureType type = GetType();

    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];
    const bool isRwTexture = m_textureDesc.imageUsage[IU_STORAGE];
    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool isBlended = m_textureDesc.IsBlended();
    const bool isSrgb = m_textureDesc.IsSrgb();

    const bool hasMipmaps = m_textureDesc.HasMipmaps();
    const uint32 numMipmaps = m_textureDesc.NumMipmaps();
    const uint32 numFaces = m_textureDesc.NumFaces();

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
        m_usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else
    {
        m_usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (hasMipmaps)
    {
        /* Mipmapped image needs linear blitting. */
        vkFormatFeatures |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        m_usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

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

    RendererResult formatSupportResult = GetRenderBackend()->GetDevice()->GetFeatures().GetImageFormatProperties(
        vkFormat,
        vkImageType,
        m_tiling,
        m_usageFlags,
        vkImageCreateFlags,
        &vkImageFormatProperties);

    if (!formatSupportResult)
    {
        HYP_GFX_CHECK(formatSupportResult);
    }

    const QueueFamilyIndices& qfIndices = GetRenderBackend()->GetDevice()->GetQueueFamilyIndices();
    const uint32 imageFamilyIndices[] = { qfIndices.graphicsFamily.Get(), qfIndices.computeFamily.Get() };

    VkImageCreateInfo imageInfo { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = vkImageType;
    imageInfo.extent.width = extent.x;
    imageInfo.extent.height = extent.y;
    imageInfo.extent.depth = extent.z;
    imageInfo.mipLevels = numMipmaps;
    imageInfo.arrayLayers = numFaces;
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

    VULKAN_CHECK_MSG(
        vmaCreateImage(
            GetRenderBackend()->GetDevice()->GetAllocator(),
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

    HYPERION_RETURN_OK;
}

RendererResult VulkanGpuImage::Destroy()
{
    RendererResult result;

    if (IsCreated())
    {
        if (m_allocation != VK_NULL_HANDLE)
        {
            HYP_GFX_ASSERT(m_isHandleOwned, "If allocation is not VK_NULL_HANDLE, is_handle_owned should be true");

            vmaDestroyImage(GetRenderBackend()->GetDevice()->GetAllocator(), m_handle, m_allocation);
            m_allocation = VK_NULL_HANDLE;
        }

        m_handle = VK_NULL_HANDLE;

        // reset back to default
        m_isHandleOwned = true;

        m_resourceState = RS_UNDEFINED;
        m_subResourceStates.Clear();

        HYPERION_RETURN_OK;
    }

    return result;
}

RendererResult VulkanGpuImage::Resize(const Vec3u& extent)
{
    if (extent == m_textureDesc.extent)
    {
        HYPERION_RETURN_OK;
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

        HYP_GFX_CHECK(Destroy());
        HYP_GFX_CHECK(Create());

        if (previousResourceState != RS_UNDEFINED)
        {
            SetResourceState(RS_UNDEFINED);

            FrameBase* frame = GetRenderBackend()->GetCurrentFrame();
            RenderQueue& renderQueue = frame->renderQueue;
            renderQueue << ::hyperion::InsertBarrier(HandleFromThis(), previousResourceState);
        }
    }

    return {};
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
    CommandBufferBase* commandBuffer,
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
    CommandBufferBase* commandBuffer,
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
    for (int mipLevel = int(subResource.baseMipLevel); mipLevel < int(subResource.baseMipLevel) + int(MathUtil::Min(subResource.numLevels, NumMipmaps())); mipLevel++)
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
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        GetVkShaderStageMask(prevResourceState, true, shaderModuleType),
        GetVkShaderStageMask(newState, false, shaderModuleType),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    if (newState == m_resourceState)
    {
        for (int mipLevel = int(subResource.baseMipLevel); mipLevel < int(subResource.baseMipLevel) + int(MathUtil::Min(subResource.numLevels, NumMipmaps())); mipLevel++)
        {
            for (int arrayLayer = int(subResource.baseArrayLayer); arrayLayer < int(subResource.baseArrayLayer) + int(MathUtil::Min(subResource.numLayers, NumLayers())); arrayLayer++)
            {
                const uint64 subResourceKey = GetImageSubResourceKey(arrayLayer, mipLevel);

                m_subResourceStates.Erase(subResourceKey);
            }
        }

        return;
    }
    else if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMipmaps()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumLayers())
    {
        // If all subresources will be set, just set the whole resource state
        SetResourceState(newState);

        return;
    }

    for (int mipLevel = int(subResource.baseMipLevel); mipLevel < int(subResource.baseMipLevel) + int(MathUtil::Min(subResource.numLevels, NumMipmaps())); mipLevel++)
    {
        for (int arrayLayer = int(subResource.baseArrayLayer); arrayLayer < int(subResource.baseArrayLayer) + int(MathUtil::Min(subResource.numLayers, NumLayers())); arrayLayer++)
        {
            const uint64 subResourceKey = GetImageSubResourceKey(arrayLayer, mipLevel);

            m_subResourceStates.Set(subResourceKey, newState);
        }
    }
}

RendererResult VulkanGpuImage::Blit(
    CommandBufferBase* commandBuffer,
    const GpuImageBase* src)
{
    return Blit(
        commandBuffer,
        src,
        Rect<uint32> { 0, 0, src->GetExtent().x, src->GetExtent().y },
        Rect<uint32> { 0, 0, m_textureDesc.extent.x, m_textureDesc.extent.y });
}

RendererResult VulkanGpuImage::Blit(
    CommandBufferBase* commandBuffer,
    const GpuImageBase* srcImage,
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
    CommandBufferBase* commandBuffer,
    const GpuImageBase* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect)
{
    const uint32 numFaces = MathUtil::Min(NumFaces(), srcImage->NumFaces());

    for (uint32 face = 0; face < numFaces; face++)
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

        /* Blit src -> dst */
        const VkImageBlit blit {
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
            VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
            VULKAN_CAST(srcImage)->GetVulkanHandle(),
            GetVkImageLayout(srcResourceState),
            m_handle,
            GetVkImageLayout(dstResourceState),
            1, &blit,
            ToVkFilter(GetMinFilterMode()));
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanGpuImage::Blit(
    CommandBufferBase* commandBuffer,
    const GpuImageBase* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect,
    uint32 srcMip,
    uint32 dstMip,
    uint32 srcFace,
    uint32 dstFace)
{
    const uint32 numFaces = MathUtil::Min(NumFaces(), srcImage->NumFaces());

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

    /* Blit src -> dst */
    const VkImageBlit blit {
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
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VULKAN_CAST(srcImage)->GetVulkanHandle(),
        GetVkImageLayout(srcResourceState),
        m_handle,
        GetVkImageLayout(dstResourceState),
        1, &blit,
        ToVkFilter(GetMinFilterMode()));

    HYPERION_RETURN_OK;
}

void VulkanGpuImage::CopyFromBuffer(CommandBufferBase* commandBuffer, const GpuBufferBase* srcBuffer) const
{
    const auto flags = m_textureDesc.IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspectFlagBits =
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    // copy from staging to image
    const uint32 numFaces = m_textureDesc.NumFaces();
    const uint32 bufferOffsetStep = uint32(m_size) / numFaces;

    for (uint32 i = 0; i < numFaces; i++)
    {
        VkBufferImageCopy region {};
        region.bufferOffset = i * bufferOffsetStep;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspectFlagBits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_textureDesc.extent.x, m_textureDesc.extent.y, m_textureDesc.extent.z };

        vkCmdCopyBufferToImage(
            VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
            VULKAN_CAST(srcBuffer)->GetVulkanHandle(),
            m_handle,
            GetVkImageLayout(m_resourceState),
            1,
            &region);
    }
}

void VulkanGpuImage::CopyToBuffer(CommandBufferBase* commandBuffer, GpuBufferBase* dstBuffer) const
{
    const auto flags = m_textureDesc.IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspectFlagBits =
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    // copy from staging to image
    const uint32 numFaces = NumFaces();
    const uint32 bufferOffsetStep = uint32(m_size) / numFaces;

    for (uint32 faceIndex = 0; faceIndex < numFaces; faceIndex++)
    {
        VkBufferImageCopy region {};
        region.bufferOffset = faceIndex * bufferOffsetStep;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspectFlagBits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = faceIndex;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_textureDesc.extent.x, m_textureDesc.extent.y, m_textureDesc.extent.z };

        vkCmdCopyImageToBuffer(
            VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
            m_handle,
            GetVkImageLayout(m_resourceState),
            VULKAN_CAST(dstBuffer)->GetVulkanHandle(),
            1,
            &region);
    }
}

GpuImageViewRef VulkanGpuImage::MakeLayerImageView(uint32 layerIndex) const
{
    if (m_handle == VK_NULL_HANDLE)
    {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to create image view on uninitialized image");

        return GpuImageViewRef();
    }

    return GetRenderBackend()->MakeImageView(
        HandleFromThis(),
        0,
        NumMipmaps(),
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
        vmaSetAllocationName(GetRenderBackend()->GetDevice()->GetAllocator(), m_allocation, strName);
    }

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(GetRenderBackend()->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanGpuImage

} // namespace hyperion
