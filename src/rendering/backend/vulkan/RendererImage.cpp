/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <util/img/ImageUtil.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/Pair.hpp>

#include <core/functional/Proc.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;
    
namespace renderer {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

extern VkImageLayout GetVkImageLayout(ResourceState);
extern VkAccessFlags GetVkAccessMask(ResourceState);
extern VkPipelineStageFlags GetVkShaderStageMask(ResourceState, bool, ShaderModuleType);


#if 0
RendererResult ImagePlatformImpl<Platform::VULKAN>::ConvertTo32BPP(
    const TextureData *in_texture_data,
    VkImageType image_type,
    VkImageCreateFlags image_create_flags,
    VkImageFormatProperties *out_image_format_properties,
    VkFormat *out_format,
    UniquePtr<TextureData> &out_texture_data
)
{
    constexpr uint8 new_bpp = 4;

    const uint32 num_faces = NumFaces();

    const TextureDesc &current_desc = GetTextureDesc();

    const uint32 size = m_size;
    const uint32 face_offset_step = size / num_faces;

    const uint8 bpp = m_bpp;

    const uint32 new_size = num_faces * new_bpp * current_desc.extent.x * current_desc.extent.y * current_desc.extent.z;
    const uint32 new_face_offset_step = new_size / num_faces;
    
    const InternalFormat new_format = FormatChangeNumComponents(current_desc.format, new_bpp);

    if (in_texture_data != nullptr) {
        AssertThrow(in_texture_data->buffer.Size() == size);

        ByteBuffer new_byte_buffer(new_size);

        for (uint32 i = 0; i < num_faces; i++) {
            ImageUtil::ConvertBPP(
                current_desc.extent.x, current_desc.extent.y, current_desc.extent.z,
                bpp, new_bpp,
                &in_texture_data->buffer.Data()[i * face_offset_step],
                &new_byte_buffer.Data()[i * new_face_offset_step]
            );
        }

        m_texture_desc = TextureDesc {
            current_desc.type,
            new_format,
            current_desc.extent,
            current_desc.filter_mode_min,
            current_desc.filter_mode_mag,
            current_desc.wrap_mode,
            current_desc.num_layers
        };

        out_texture_data.Emplace(TextureData {
            m_texture_desc,
            std::move(new_byte_buffer)
        });
    }

    m_bpp = new_bpp;
    m_size = new_size;

    *out_format = helpers::ToVkFormat(m_texture_desc.format);

    HYPERION_RETURN_OK;
}
#endif

#pragma endregion VulkanImagePlatformImpl

#pragma region VulkanImage

VulkanImage::VulkanImage(const TextureDesc &texture_desc)
    : ImageBase(texture_desc),
      m_bpp(NumComponents(GetBaseFormat(texture_desc.format)))
{
    m_size = texture_desc.GetByteSize();
}

VulkanImage::~VulkanImage()
{
    AssertThrow(m_handle == VK_NULL_HANDLE);
}

bool VulkanImage::IsCreated() const
    { return m_handle != VK_NULL_HANDLE; }

RendererResult VulkanImage::GenerateMipmaps(CommandBufferBase *command_buffer)
{
    if (m_handle == VK_NULL_HANDLE) {
        return HYP_MAKE_ERROR(RendererError, "Cannot generate mipmaps on uninitialized image");
    }

    const uint32 num_faces = NumFaces();
    const uint32 num_mipmaps = NumMipmaps();

    for (uint32 face = 0; face < num_faces; face++) {
        for (int32 i = 1; i < int32(num_mipmaps + 1); i++) {
            const int mip_width = int(helpers::MipmapSize(m_texture_desc.extent.x, i)),
                mip_height = int(helpers::MipmapSize(m_texture_desc.extent.y, i)),
                mip_depth = int(helpers::MipmapSize(m_texture_desc.extent.z, i));

            /* Memory barrier for transfer - note that after generating the mipmaps,
                we'll still need to transfer into a layout primed for reading from shaders. */

            const ImageSubResource src {
                .flags = m_texture_desc.IsDepthStencil()
                    ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                    : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
                .base_array_layer = face,
                .base_mip_level = uint32(i - 1)
            };

            const ImageSubResource dst {
                .flags = src.flags,
                .base_array_layer = src.base_array_layer,
                .base_mip_level = uint32(i)
            };
            
            InsertSubResourceBarrier(
                command_buffer,
                src,
                ResourceState::COPY_SRC,
                ShaderModuleType::UNSET
            );

            if (i == int32(num_mipmaps)) {
                if (face == num_faces - 1) {
                    /* all individual subresources have been set so we mark the whole
                     * resource as being int his state */
                    SetResourceState(ResourceState::COPY_SRC);
                }

                break;
            }

            const VkImageAspectFlags aspect_flag_bits = 
                (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
                | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
                | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
            /* Blit src -> dst */
            const VkImageBlit blit {
                .srcSubresource = {
                    .aspectMask = aspect_flag_bits,
                    .mipLevel = src.base_mip_level,
                    .baseArrayLayer = src.base_array_layer,
                    .layerCount = src.num_layers
                },
                .srcOffsets = {
                    { 0, 0, 0 },
                    {
                        int32(helpers::MipmapSize(m_texture_desc.extent.x, i - 1)),
                        int32(helpers::MipmapSize(m_texture_desc.extent.y, i - 1)),
                        int32(helpers::MipmapSize(m_texture_desc.extent.z, i - 1))
                    }
                },
                .dstSubresource = {
                    .aspectMask  = aspect_flag_bits,
                    .mipLevel = dst.base_mip_level,
                    .baseArrayLayer = dst.base_array_layer,
                    .layerCount = dst.num_layers
                },
                .dstOffsets = {
                    { 0, 0, 0 },
                    {
                        mip_width,
                        mip_height,
                        mip_depth
                    }
                }
            };

            vkCmdBlitImage(
                static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
                m_handle,
                GetVkImageLayout(ResourceState::COPY_SRC),
                m_handle,
                GetVkImageLayout(ResourceState::COPY_DST),
                1, &blit,
                m_texture_desc.IsDepthStencil() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR // TODO: base on filter mode
            );
        }
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanImage::Destroy()
{
    RendererResult result;

    if (IsCreated()) {
        if (m_allocation != VK_NULL_HANDLE) {
            AssertThrowMsg(m_is_handle_owned, "If allocation is not VK_NULL_HANDLE, is_handle_owned should be true");

            vmaDestroyImage(GetRenderingAPI()->GetDevice()->GetAllocator(), m_handle, m_allocation);
            m_allocation = VK_NULL_HANDLE;
        }

        m_handle = VK_NULL_HANDLE;

        // reset back to default
        m_is_handle_owned = true;

        HYPERION_RETURN_OK;
    }

    return result;
}

RendererResult VulkanImage::Create()
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    return Create(ResourceState::UNDEFINED);
}

RendererResult VulkanImage::Create(ResourceState initial_state)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    VkImageLayout initial_layout = GetVkImageLayout(initial_state);

    if (!m_is_handle_owned) {
        AssertThrowMsg(m_handle != VK_NULL_HANDLE, "If is_handle_owned is set to false, the image handle must not be VK_NULL_HANDLE.");

        HYPERION_RETURN_OK;
    }

    const Vec3u extent = GetExtent();
    const InternalFormat format = GetTextureFormat();
    const ImageType type = GetType();

    const bool is_attachment_texture = m_texture_desc.image_format_capabilities[ImageFormatCapabilities::ATTACHMENT];
    const bool is_rw_texture = m_texture_desc.image_format_capabilities[ImageFormatCapabilities::STORAGE];
    const bool is_depth_stencil = m_texture_desc.IsDepthStencil();
    const bool is_blended = m_texture_desc.IsBlended();
    const bool is_srgb = m_texture_desc.IsSRGB();

    const bool is_cubemap = m_texture_desc.IsTextureCube();

    const bool has_mipmaps = m_texture_desc.HasMipmaps();
    const uint32 num_mipmaps = m_texture_desc.NumMipmaps();
    const uint32 num_faces = m_texture_desc.NumFaces();

    if (extent.Volume() == 0) {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    VkFormat vk_format = helpers::ToVkFormat(format);
    VkImageType vk_image_type = helpers::ToVkType(type);
    VkImageCreateFlags vk_image_create_flags = 0;
    VkFormatFeatureFlags vk_format_features = 0;
    VkImageFormatProperties vk_image_format_properties { };

    m_tiling = VK_IMAGE_TILING_OPTIMAL;
    m_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (is_attachment_texture) {
        m_usage_flags |= (is_depth_stencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (is_rw_texture) {
        m_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT;
    } else {
        m_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (has_mipmaps) {
        /* Mipmapped image needs linear blitting. */
        vk_format_features |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        m_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (GetMinFilterMode()) {
        case FilterMode::TEXTURE_FILTER_LINEAR: // fallthrough
        case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        default:
            break;
        }
    }

    if (is_blended) {
        HYP_LOG(RenderingBackend, Debug, "Image requires blending, enabling format flag...");

        vk_format_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (is_cubemap) {
        HYP_LOG(RenderingBackend, Debug, "Creating cubemap, enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.");

        vk_image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    RendererResult format_support_result = GetRenderingAPI()->GetDevice()->GetFeatures().GetImageFormatProperties(
        vk_format,
        vk_image_type,
        m_tiling,
        m_usage_flags,
        vk_image_create_flags,
        &vk_image_format_properties
    );

    if (!format_support_result) {
        HYP_BREAKPOINT;
        HYPERION_BUBBLE_ERRORS(format_support_result);
    }

    const QueueFamilyIndices &qf_indices = GetRenderingAPI()->GetDevice()->GetQueueFamilyIndices();
    const uint32 image_family_indices[] = { qf_indices.graphics_family.Get(), qf_indices.compute_family.Get() };

    VkImageCreateInfo image_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.imageType = vk_image_type;
    image_info.extent.width = extent.x;
    image_info.extent.height = extent.y;
    image_info.extent.depth = extent.z;
    image_info.mipLevels = num_mipmaps;
    image_info.arrayLayers = num_faces;
    image_info.format = vk_format;
    image_info.tiling = m_tiling;
    image_info.initialLayout = initial_layout;
    image_info.usage = m_usage_flags;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = vk_image_create_flags;
    image_info.pQueueFamilyIndices = image_family_indices;
    image_info.queueFamilyIndexCount = uint32(std::size(image_family_indices));

    VmaAllocationCreateInfo alloc_info { };
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    HYPERION_VK_CHECK_MSG(
        vmaCreateImage(
            GetRenderingAPI()->GetDevice()->GetAllocator(),
            &image_info,
            &alloc_info,
            &m_handle,
            &m_allocation,
            nullptr
        ),
        "Failed to create gpu image!"
    );

    HYPERION_RETURN_OK;
}

void VulkanImage::SetResourceState(ResourceState new_state)
{
    m_resource_state = new_state;

    m_sub_resources.Clear();
}

ResourceState VulkanImage::GetSubResourceState(const ImageSubResource &sub_resource) const
{
    const auto it = m_sub_resources.Find(sub_resource);

    if (it == m_sub_resources.End()) {
        return m_resource_state;
    }

    return it->second;
}

void VulkanImage::SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state)
{    
    m_sub_resources[sub_resource] = new_state;
}

void VulkanImage::InsertBarrier(
    CommandBufferBase *command_buffer,
    ResourceState new_state,
    ShaderModuleType shader_module_type
)
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_NONE;

    if (m_texture_desc.IsDepthStencil()) {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL;
    } else {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    }

    InsertBarrier(
        command_buffer,
        ImageSubResource {
            .flags = flags,
            .num_layers = VK_REMAINING_ARRAY_LAYERS,
            .num_levels = VK_REMAINING_MIP_LEVELS
        },
        new_state,
        shader_module_type
    );
}

void VulkanImage::InsertBarrier(
    CommandBufferBase *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state,
    ShaderModuleType shader_module_type
)
{
    /* Clear any sub-resources that are in a separate state */
    if (!m_sub_resources.Empty()) {
        m_sub_resources.Clear();
    }

    if (m_handle == VK_NULL_HANDLE) {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to insert a resource barrier but image was not defined"
        );

        return;
    }

    const VkImageAspectFlags aspect_flag_bits = 
        (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range { };
    range.aspectMask = aspect_flag_bits;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount = sub_resource.num_layers;
    range.baseMipLevel = sub_resource.base_mip_level;
    range.levelCount = sub_resource.num_levels;

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = GetVkImageLayout(m_resource_state);
    barrier.newLayout = GetVkImageLayout(new_state);
    barrier.srcAccessMask = GetVkAccessMask(m_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.image = m_handle;
    barrier.subresourceRange = range;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        GetVkShaderStageMask(m_resource_state, true, shader_module_type),
        GetVkShaderStageMask(new_state, false, shader_module_type),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    m_resource_state = new_state;
}

void VulkanImage::InsertSubResourceBarrier(
    CommandBufferBase *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state,
    ShaderModuleType shader_module_type
)
{
    if (m_handle == VK_NULL_HANDLE) {
        HYP_LOG(
            RenderingBackend,
            Debug,
            "Attempt to insert a resource barrier but image was not defined"
        );

        return;
    }

    ResourceState prev_resource_state = m_resource_state;

    auto it = m_sub_resources.Find(sub_resource);

    if (it != m_sub_resources.End()) {
        prev_resource_state = it->second;
    }

    const VkImageAspectFlags aspect_flag_bits = 
        (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range { };
    range.aspectMask = aspect_flag_bits;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount = sub_resource.num_layers;
    range.baseMipLevel = sub_resource.base_mip_level;
    range.levelCount = sub_resource.num_levels;

    VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = GetVkImageLayout(prev_resource_state);
    barrier.newLayout = GetVkImageLayout(new_state);
    barrier.srcAccessMask = GetVkAccessMask(prev_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.image = m_handle;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = range;

    vkCmdPipelineBarrier(
        static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        GetVkShaderStageMask(prev_resource_state, true, shader_module_type),
        GetVkShaderStageMask(new_state, false, shader_module_type),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    m_sub_resources.Insert({ sub_resource, new_state });
}

RendererResult VulkanImage::Blit(
    CommandBufferBase *command_buffer,
    const ImageBase *src_image,
    Rect<uint32> src_rect,
    Rect<uint32> dst_rect,
    uint32 src_mip,
    uint32 dst_mip
)
{
    const uint32 num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint32 face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->GetTextureDesc().IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer   = face,
            .base_mip_level     = src_mip
        };

        const ImageSubResource dst {
            .flags = m_texture_desc.IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level   = dst_mip
        };

        const VkImageAspectFlags aspect_flag_bits = 
            (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
        
        /* Blit src -> dst */
        const VkImageBlit blit {
            .srcSubresource = {
                .aspectMask     = aspect_flag_bits,
                .mipLevel       = src_mip,
                .baseArrayLayer = src.base_array_layer,
                .layerCount     = src.num_layers
            },
            .srcOffsets = {
                { int32(src_rect.x0), int32(src_rect.y0), 0 },
                { int32(src_rect.x1), int32(src_rect.y1), 1 }
            },
            .dstSubresource = {
                .aspectMask     = aspect_flag_bits,
                .mipLevel       = dst_mip,
                .baseArrayLayer = dst.base_array_layer,
                .layerCount     = dst.num_layers
            },
            .dstOffsets = {
                { int32(dst_rect.x0), int32(dst_rect.y0), 0 },
                { int32(dst_rect.x1), int32(dst_rect.y1), 1 }
            }
        };

        vkCmdBlitImage(
            static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
            static_cast<const VulkanImage *>(src_image)->GetVulkanHandle(),
            GetVkImageLayout(static_cast<const VulkanImage *>(src_image)->GetResourceState()),
            m_handle,
            GetVkImageLayout(GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(GetMinFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanImage::Blit(
    CommandBufferBase *command_buffer,
    const ImageBase *src_image,
    Rect<uint32> src_rect,
    Rect<uint32> dst_rect
)
{
    const uint32 num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint32 face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->GetTextureDesc().IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer   = face,
            .base_mip_level     = 0
        };

        const ImageSubResource dst {
            .flags = m_texture_desc.IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level   = 0
        };

        const VkImageAspectFlags aspect_flag_bits = 
            (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
        
        /* Blit src -> dst */
        const VkImageBlit blit {
            .srcSubresource = {
                .aspectMask     = aspect_flag_bits,
                .mipLevel       = src.base_mip_level,
                .baseArrayLayer = src.base_array_layer,
                .layerCount     = src.num_layers
            },
            .srcOffsets = {
                { (int32_t) src_rect.x0, (int32_t) src_rect.y0, 0 },
                { (int32_t) src_rect.x1, (int32_t) src_rect.y1, 1 }
            },
            .dstSubresource = {
                .aspectMask = aspect_flag_bits,
                .mipLevel = dst.base_mip_level,
                .baseArrayLayer = dst.base_array_layer,
                .layerCount = dst.num_layers
            },
            .dstOffsets = {
                { (int32_t) dst_rect.x0, (int32_t) dst_rect.y0, 0 },
                { (int32_t) dst_rect.x1, (int32_t) dst_rect.y1, 1 }
            }
        };

        vkCmdBlitImage(
            static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
            static_cast<const VulkanImage *>(src_image)->GetVulkanHandle(),
            GetVkImageLayout(static_cast<const VulkanImage *>(src_image)->GetResourceState()),
            m_handle,
            GetVkImageLayout(GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(GetMinFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanImage::Blit(
    CommandBufferBase *command_buffer,
    const ImageBase *src
)
{
    return Blit(
        command_buffer,
        src,
        Rect<uint32> {
            .x0 = 0,
            .y0 = 0,
            .x1 = src->GetExtent().x,
            .y1 = src->GetExtent().y
        },
        Rect<uint32> {
            .x0 = 0,
            .y0 = 0,
            .x1 = m_texture_desc.extent.x,
            .y1 = m_texture_desc.extent.y
        }
    );
}

void VulkanImage::CopyFromBuffer(CommandBufferBase *command_buffer, const GPUBufferBase *src_buffer) const
{
    const auto flags = m_texture_desc.IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspect_flag_bits = 
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
    // copy from staging to image
    const uint32 num_faces = m_texture_desc.NumFaces();
    const uint32 buffer_offset_step = uint32(m_size) / num_faces;

    for (uint32 i = 0; i < num_faces; i++) {
        VkBufferImageCopy region { };
        region.bufferOffset = i * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_texture_desc.extent.x, m_texture_desc.extent.y, m_texture_desc.extent.z };

        vkCmdCopyBufferToImage(
            static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
            static_cast<const VulkanGPUBuffer *>(src_buffer)->GetVulkanHandle(),
            m_handle,
            GetVkImageLayout(m_resource_state),
            1,
            &region
        );
    }
}

void VulkanImage::CopyToBuffer(CommandBufferBase *command_buffer, GPUBufferBase *dst_buffer) const
{
    const auto flags = m_texture_desc.IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspect_flag_bits = 
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
    // copy from staging to image
    const uint32 num_faces = NumFaces();
    const uint32 buffer_offset_step = uint32(m_size) / num_faces;

    for (uint32 face_index = 0; face_index < num_faces; face_index++) {
        VkBufferImageCopy region { };
        region.bufferOffset = face_index * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = face_index;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_texture_desc.extent.x, m_texture_desc.extent.y, m_texture_desc.extent.z };

        vkCmdCopyImageToBuffer(
            static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
            m_handle,
            GetVkImageLayout(m_resource_state),
            static_cast<VulkanGPUBuffer *>(dst_buffer)->GetVulkanHandle(),
            1,
            &region
        );
    }
}

#pragma endregion VulkanImage

} // namespace renderer
} // namespace hyperion
