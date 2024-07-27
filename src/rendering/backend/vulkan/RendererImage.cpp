/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <util/img/ImageUtil.hpp>
#include <core/system/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

extern VkImageLayout GetVkImageLayout(ResourceState);
extern VkAccessFlags GetVkAccessMask(ResourceState);
extern VkPipelineStageFlags GetVkShaderStageMask(ResourceState, bool, ShaderModuleType);

#pragma region ImagePlatformImpl

Result ImagePlatformImpl<Platform::VULKAN>::ConvertTo32BPP(
    Device<Platform::VULKAN> *device,
    VkImageType image_type,
    VkImageCreateFlags image_create_flags,
    VkImageFormatProperties *out_image_format_properties,
    VkFormat *out_format
)
{
    constexpr uint8 new_bpp = 4;

    const TextureDesc &current_desc = self->GetTextureDesc();

    const uint32 size = self->m_size;
    const uint32 face_offset_step = size / current_desc.num_faces;

    const uint8 bpp = self->m_bpp;

    const uint32 new_size = current_desc.num_faces * new_bpp * current_desc.extent.width * current_desc.extent.height * current_desc.extent.depth;
    const uint32 new_face_offset_step = new_size / current_desc.num_faces;
    
    const InternalFormat new_format = FormatChangeNumComponents(current_desc.format, new_bpp);

    if (self->HasAssignedImageData()) {
        const auto ref = self->GetStreamedData()->AcquireRef();
        const TextureData &texture_data = ref->GetTextureData();

        AssertThrow(texture_data.buffer.Size() == size);

        ByteBuffer new_byte_buffer(new_size);

        for (uint32 i = 0; i < current_desc.num_faces; i++) {
            ImageUtil::ConvertBPP(
                current_desc.extent.width, current_desc.extent.height, current_desc.extent.depth,
                bpp, new_bpp,
                &texture_data.buffer.Data()[i * face_offset_step],
                &new_byte_buffer.Data()[i * new_face_offset_step]
            );
        }

        self->m_streamed_data.Reset(new StreamedTextureData(TextureData {
            TextureDesc {
                current_desc.type,
                new_format,
                current_desc.extent,
                current_desc.filter_mode_min,
                current_desc.filter_mode_mag,
                current_desc.wrap_mode,
                current_desc.num_layers,
                current_desc.num_faces
            },
            std::move(new_byte_buffer)
        }));

        self->m_texture_desc = self->m_streamed_data->GetTextureDesc();
    }

    self->m_bpp = new_bpp;
    self->m_size = new_size;

    *out_format = helpers::ToVkFormat(self->m_texture_desc.format);

    HYPERION_RETURN_OK;
}

Result ImagePlatformImpl<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    VkImageLayout initial_layout,
    VkImageCreateInfo *out_image_info
)
{
    if (!is_handle_owned) {
        AssertThrowMsg(handle != VK_NULL_HANDLE, "If is_handle_owned is set to false, the image handle must not be VK_NULL_HANDLE.");

        HYPERION_RETURN_OK;
    }

    const Extent3D extent = self->GetExtent();
    const InternalFormat format = self->GetTextureFormat();
    const ImageType type = self->GetType();

    const bool is_attachment_texture = self->IsAttachmentTexture();
    const bool is_rw_texture = self->IsRWTexture();

    const bool is_depth_stencil = self->IsDepthStencil();
    const bool is_blended = self->IsBlended();
    const bool is_srgb = self->IsSRGB();

    const bool is_cubemap = self->IsTextureCube();

    const bool has_mipmaps = self->HasMipmaps();
    const uint32 num_mipmaps = self->NumMipmaps();

    const uint32 num_faces = self->NumFaces();

    if (extent.Size() == 0) {
        return Result { Result::RENDERER_ERR, "Invalid image extent - width*height*depth cannot equal zero" };
    }

    VkFormat vk_format = helpers::ToVkFormat(format);
    VkImageType vk_image_type = helpers::ToVkType(type);
    VkImageCreateFlags vk_image_create_flags = 0;
    VkFormatFeatureFlags vk_format_features = 0;
    VkImageFormatProperties vk_image_format_properties { };

    tiling = VK_IMAGE_TILING_OPTIMAL;

    if (is_attachment_texture) {
        usage_flags |= (is_depth_stencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (is_rw_texture) {
        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
    } else {
        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (has_mipmaps) {
        /* Mipmapped image needs linear blitting. */
        vk_format_features |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (self->GetMinFilterMode()) {
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
        HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Image requires blending, enabling format flag...");

        vk_format_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (is_cubemap) {
        HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Creating cubemap, enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.");

        vk_image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    auto format_support_result = device->GetFeatures().GetImageFormatProperties(
        vk_format,
        vk_image_type,
        tiling,
        usage_flags,
        vk_image_create_flags,
        &vk_image_format_properties
    );

    if (!format_support_result) {
        // try a series of fixes to get the image in a valid state.

        std::vector<std::pair<const char *, std::function<Result()>>> potential_fixes;

        if (!IsDepthFormat(self->GetTextureFormat())) {
            // convert to 32bpp image
            if (self->GetBPP() != 4) {
                potential_fixes.emplace_back(std::make_pair(
                    "Convert to 32-bpp image",
                    [&]() -> Result
                    {
                        return ConvertTo32BPP(
                            device,
                            vk_image_type,
                            vk_image_create_flags,
                            &vk_image_format_properties,
                            &vk_format
                        );
                    }
                ));
            }
        }

        for (auto &fix : potential_fixes) {
            HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Attempting fix '{}'...\n", fix.first);

            auto fix_result = fix.second();

            if (!fix_result) {
                HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Fix '{}' failed: {}", fix.first, fix_result.message);

                continue;
            }

            // try checking format support result again
            if ((format_support_result = device->GetFeatures().GetImageFormatProperties(
                vk_format, vk_image_type, tiling, usage_flags, vk_image_create_flags, &vk_image_format_properties))) {
                
                HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Fix '{}' succeeded.\n", fix.first);

                break;
            }

            HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Fix '{}' failed: {}", fix.first, format_support_result.message);
        }

        HYPERION_BUBBLE_ERRORS(format_support_result);
    }

    // if (format_features != 0) {
    //     if (!device->GetFeatures().IsSupportedFormat(format, m_internal_info.tiling, format_features)) {
    //         DebugLog(
    //             LogType::Error,
    //             "Device does not support the format %u with requested tiling %d and format features %d\n",
    //             static_cast<uint>(format),
    //             static_cast<int>(m_internal_info.tiling),
    //             static_cast<int>(format_features)
    //         );

    //         HYP_BREAKPOINT;

    //         return Result(Result::RENDERER_ERR, "Format does not support requested features.");
    //     }
    // }

    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32 image_family_indices[] = { qf_indices.graphics_family.Get(), qf_indices.compute_family.Get() };

    VkImageCreateInfo image_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.imageType = vk_image_type;
    image_info.extent.width = extent.width;
    image_info.extent.height = extent.height;
    image_info.extent.depth = extent.depth;
    image_info.mipLevels = num_mipmaps;
    image_info.arrayLayers = num_faces;
    image_info.format = vk_format;
    image_info.tiling = tiling;
    image_info.initialLayout = initial_layout;
    image_info.usage = usage_flags;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = vk_image_create_flags;
    image_info.pQueueFamilyIndices = image_family_indices;
    image_info.queueFamilyIndexCount = uint32(std::size(image_family_indices));

    *out_image_info = image_info;

    VmaAllocationCreateInfo alloc_info { };
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    HYPERION_VK_CHECK_MSG(
        vmaCreateImage(
            device->GetAllocator(),
            &image_info,
            &alloc_info,
            &handle,
            &allocation,
            nullptr
        ),
        "Failed to create gpu image!"
    );

    HYPERION_RETURN_OK;
}

Result ImagePlatformImpl<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (allocation != VK_NULL_HANDLE) {
        AssertThrowMsg(is_handle_owned, "If allocation is not VK_NULL_HANDLE, is_handle_owned should be true");

        vmaDestroyImage(device->GetAllocator(), handle, allocation);
        allocation = VK_NULL_HANDLE;
    }

    handle = VK_NULL_HANDLE;

    // reset back to default
    is_handle_owned = true;

    HYPERION_RETURN_OK;
}

void ImagePlatformImpl<Platform::VULKAN>::SetResourceState(ResourceState new_state)
{
    resource_state = new_state;

    sub_resources.Clear();
}

void ImagePlatformImpl<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    ResourceState new_state,
    ImageSubResourceFlagBits flags
)
{
    InsertBarrier(
        command_buffer,
        ImageSubResource {
            .flags = flags,
            .num_layers = VK_REMAINING_ARRAY_LAYERS,
            .num_levels = VK_REMAINING_MIP_LEVELS
        },
        new_state
    );
}

void ImagePlatformImpl<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    /* Clear any sub-resources that are in a separate state */
    if (!sub_resources.Empty()) {
        sub_resources.Clear();
    }

    if (handle == VK_NULL_HANDLE) {
        HYP_LOG(
            RenderingBackend,
            LogLevel::WARNING,
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
    barrier.oldLayout = GetVkImageLayout(resource_state);
    barrier.newLayout = GetVkImageLayout(new_state);
    barrier.srcAccessMask = GetVkAccessMask(resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.image = handle;
    barrier.subresourceRange = range;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetPlatformImpl().command_buffer,
        GetVkShaderStageMask(resource_state, true, ShaderModuleType::UNSET),
        GetVkShaderStageMask(new_state, false, ShaderModuleType::UNSET),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    resource_state = new_state;
}

void ImagePlatformImpl<Platform::VULKAN>::InsertSubResourceBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    if (handle == VK_NULL_HANDLE) {
        HYP_LOG(
            RenderingBackend,
            LogLevel::DEBUG,
            "Attempt to insert a resource barrier but image was not defined"
        );

        return;
    }

    ResourceState prev_resource_state = resource_state;

    auto it = sub_resources.Find(sub_resource);

    if (it != sub_resources.End()) {
        prev_resource_state = it->second;
    }

    const VkImageAspectFlags aspect_flag_bits = 
        (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range{};
    range.aspectMask = aspect_flag_bits;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount = sub_resource.num_layers;
    range.baseMipLevel = sub_resource.base_mip_level;
    range.levelCount = sub_resource.num_levels;

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = GetVkImageLayout(prev_resource_state);
    barrier.newLayout = GetVkImageLayout(new_state);
    barrier.srcAccessMask = GetVkAccessMask(prev_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.image = handle;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = range;

    vkCmdPipelineBarrier(
        command_buffer->GetPlatformImpl().command_buffer,
        GetVkShaderStageMask(prev_resource_state, true, ShaderModuleType::UNSET),
        GetVkShaderStageMask(new_state, false, ShaderModuleType::UNSET),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    sub_resources.Insert({ sub_resource, new_state });
}

auto ImagePlatformImpl<Platform::VULKAN>::GetSubResourceState(const ImageSubResource &sub_resource) const -> ResourceState
{
    const auto it = sub_resources.Find(sub_resource);

    if (it == sub_resources.End()) {
        return resource_state;
    }

    return it->second;
}

void ImagePlatformImpl<Platform::VULKAN>::SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state)
{
    sub_resources[sub_resource] = new_state;
}

#pragma endregion ImagePlatformImpl

#pragma region Image

template <>
Image<Platform::VULKAN>::Image(
    const TextureDesc &texture_desc
) : m_platform_impl { this },
    m_texture_desc(texture_desc),
    m_is_blended(false),
    m_is_rw_texture(false),
    m_is_attachment_texture(false),
    m_bpp(NumComponents(GetBaseFormat(texture_desc.format))),
    m_flags(IMAGE_FLAGS_NONE)
{
    m_size = GetByteSize();

    m_streamed_data.Reset(new StreamedTextureData(TextureData
    {
        m_texture_desc,
        ByteBuffer()
    }));
}

template <>
Image<Platform::VULKAN>::Image(
    const RC<StreamedTextureData> &streamed_data
) : m_platform_impl { this },
    m_streamed_data(streamed_data),
    m_is_blended(false),
    m_is_rw_texture(false),
    m_is_attachment_texture(false),
    m_bpp(0),
    m_flags(IMAGE_FLAGS_NONE)
{
    AssertThrow(m_streamed_data != nullptr);

    m_texture_desc = m_streamed_data->GetTextureDesc();
    m_bpp = NumComponents(GetBaseFormat(m_texture_desc.format));

    m_size = GetByteSize();
}

template <>
Image<Platform::VULKAN>::Image(
    RC<StreamedTextureData> &&streamed_data
) : m_platform_impl { this },
    m_streamed_data(std::move(streamed_data)),
    m_is_blended(false),
    m_is_rw_texture(false),
    m_is_attachment_texture(false),
    m_bpp(0),
    m_flags(IMAGE_FLAGS_NONE)
{
    AssertThrow(m_streamed_data != nullptr);

    m_texture_desc = m_streamed_data->GetTextureDesc();
    m_bpp = NumComponents(GetBaseFormat(m_texture_desc.format));

    m_size = GetByteSize();
}

template <>
Image<Platform::VULKAN>::Image(Image &&other) noexcept
    : m_texture_desc(other.m_texture_desc),
      m_is_blended(other.m_is_blended),
      m_is_rw_texture(other.m_is_rw_texture),
      m_is_attachment_texture(other.m_is_attachment_texture),
      m_size(other.m_size),
      m_bpp(other.m_bpp),
      m_streamed_data(std::move(other.m_streamed_data)),
      m_flags(other.m_flags)
{
    m_platform_impl = std::move(other.m_platform_impl);
    m_platform_impl.self = this;
    other.m_platform_impl = { &other };

    other.m_is_blended = false;
    other.m_is_rw_texture = false;
    other.m_is_attachment_texture = false;
    other.m_size = 0;
    other.m_bpp = 0;
}

template <>
Image<Platform::VULKAN> &Image<Platform::VULKAN>::operator=(Image &&other) noexcept
{
    m_platform_impl = std::move(other.m_platform_impl);
    m_platform_impl.self = this;
    other.m_platform_impl = { &other };

    m_texture_desc = std::move(other.m_texture_desc);
    m_is_blended = other.m_is_blended;
    m_is_rw_texture = other.m_is_rw_texture;
    m_is_attachment_texture = other.m_is_attachment_texture;
    m_size = other.m_size;
    m_bpp = other.m_bpp;
    m_streamed_data = std::move(other.m_streamed_data);
    m_flags = other.m_flags;

    other.m_is_blended = false;
    other.m_is_rw_texture = false;
    other.m_is_attachment_texture = false;
    other.m_size = 0;
    other.m_bpp = 0;

    return *this;
}

template <>
Image<Platform::VULKAN>::~Image()
{
    AssertThrow(m_platform_impl.handle == VK_NULL_HANDLE);
}

template <>
bool Image<Platform::VULKAN>::IsCreated() const
    { return m_platform_impl.handle != VK_NULL_HANDLE; }

template <>
Result Image<Platform::VULKAN>::GenerateMipmaps(
    Device<Platform::VULKAN> *device,
    CommandBuffer<Platform::VULKAN> *command_buffer
)
{
    if (m_platform_impl.handle == VK_NULL_HANDLE) {
        return { Result::RENDERER_ERR, "Cannot generate mipmaps on uninitialized image" };
    }

    const auto num_faces = NumFaces();
    const auto num_mipmaps = NumMipmaps();

    for (uint32 face = 0; face < num_faces; face++) {
        for (int32 i = 1; i < int32(num_mipmaps + 1); i++) {
            const auto mip_width = int32(helpers::MipmapSize(m_texture_desc.extent.width, i)),
                mip_height = int32(helpers::MipmapSize(m_texture_desc.extent.height, i)),
                mip_depth = int32(helpers::MipmapSize(m_texture_desc.extent.depth, i));

            /* Memory barrier for transfer - note that after generating the mipmaps,
                we'll still need to transfer into a layout primed for reading from shaders. */

            const ImageSubResource src {
                .flags = IsDepthStencil()
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
            
            m_platform_impl.InsertSubResourceBarrier(
                command_buffer,
                src,
                ResourceState::COPY_SRC
            );

            if (i == int32(num_mipmaps)) {
                if (face == num_faces - 1) {
                    /* all individual subresources have been set so we mark the whole
                     * resource as being int his state */
                    m_platform_impl.SetResourceState(ResourceState::COPY_SRC);
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
                        int32(helpers::MipmapSize(m_texture_desc.extent.width, i - 1)),
                        int32(helpers::MipmapSize(m_texture_desc.extent.height, i - 1)),
                        int32(helpers::MipmapSize(m_texture_desc.extent.depth, i - 1))
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
                command_buffer->GetPlatformImpl().command_buffer,
                m_platform_impl.handle,
                GetVkImageLayout(ResourceState::COPY_SRC),
                m_platform_impl.handle,
                GetVkImageLayout(ResourceState::COPY_DST),
                1, &blit,
                IsDepthStencil() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR // TODO: base on filter mode
            );
        }
    }

    HYPERION_RETURN_OK;
}

template <>
Result Image<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    Result result;

    if (IsCreated()) {
        HYPERION_PASS_ERRORS(m_platform_impl.Destroy(device), result);
        m_platform_impl.handle = VK_NULL_HANDLE;
    }

    if (m_streamed_data) {
        m_streamed_data->Unpage();
    }

    return result;
}

template <>
Result Image<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    VkImageCreateInfo image_info;

    return m_platform_impl.Create(device, VK_IMAGE_LAYOUT_UNDEFINED, &image_info);
}

template <>
Result Image<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance, ResourceState state)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    Result result;

    VkImageCreateInfo image_info;

    HYPERION_BUBBLE_ERRORS(m_platform_impl.Create(device, VK_IMAGE_LAYOUT_UNDEFINED, &image_info));

    /*VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = NumMipmaps();
    range.baseArrayLayer = 0;
    range.layerCount = NumFaces();*/

    const ImageSubResource sub_resource {
        .num_layers = NumFaces(),
        .num_levels = NumMipmaps()
    };

    SingleTimeCommands<Platform::VULKAN> commands { device };
    StagingBuffer<Platform::VULKAN> staging_buffer;

    if (HasAssignedImageData()) {
        auto ref = m_streamed_data->AcquireRef();
        const TextureData &texture_data = ref->GetTextureData();

        AssertThrowMsg(
            m_size == texture_data.buffer.Size(),
            "Invalid image size -- loaded data size (%llu) does not match image size (%llu)",
            texture_data.buffer.Size(),
            m_size
        );

        AssertThrowMsg(m_size % m_bpp == 0, "Invalid image size");
        AssertThrowMsg((m_size / m_bpp) % texture_data.desc.num_faces == 0, "Invalid image size");

        HYPERION_PASS_ERRORS(staging_buffer.Create(device, m_size), result);

        if (!result) {
            HYPERION_IGNORE_ERRORS(Destroy(device));

            return result;            
        }

        staging_buffer.Copy(device, m_size, texture_data.buffer.Data());

        commands.Push([&](const CommandBufferRef<Platform::VULKAN> &command_buffer)
        {
            m_platform_impl.InsertBarrier(
                command_buffer,
                sub_resource,
                ResourceState::COPY_DST
            );

            HYPERION_RETURN_OK;
        });

        // copy from staging to image
        const uint32 buffer_offset_step = uint(m_size) / texture_data.desc.num_faces;

        AssertThrowMsg(m_size % buffer_offset_step == 0, "Invalid image size");
        AssertThrowMsg(m_size / buffer_offset_step == texture_data.desc.num_faces, "Invalid image size");

        for (uint32 i = 0; i < texture_data.desc.num_faces; i++) {
            commands.Push([this, &staging_buffer, &image_info, i, buffer_offset_step](const CommandBufferRef<Platform::VULKAN> &command_buffer)
            {
                const uint32 buffer_size = staging_buffer.GetPlatformImpl().size;
                const uint32 buffer_offset_step_ = buffer_offset_step;
                const uint32 total_size = m_size;

                VkBufferImageCopy region { };
                region.bufferOffset = i * buffer_offset_step;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = i;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = { 0, 0, 0 };
                region.imageExtent = image_info.extent;

                vkCmdCopyBufferToImage(
                    command_buffer->GetPlatformImpl().command_buffer,
                    staging_buffer.GetPlatformImpl().handle,
                    m_platform_impl.handle,
                    GetVkImageLayout(m_platform_impl.GetResourceState()),
                    1,
                    &region
                );

                HYPERION_RETURN_OK;
            });
        }

        /* Generate mipmaps if it applies */
        if (HasMipmaps()) {
            /* Assuming device supports this format with linear blitting -- check is done in CreateImage() */

            /* Generate our mipmaps
            */
            commands.Push([this, device](const CommandBufferRef<Platform::VULKAN> &command_buffer)
            {
                return GenerateMipmaps(device, command_buffer);
            });
        }
    }

    /* Transition from whatever the previous layout state was to our destination state */
    commands.Push([&](const CommandBufferRef<Platform::VULKAN> &command_buffer)
    {
        m_platform_impl.InsertBarrier(
            command_buffer,
            sub_resource,
            state
        );

        HYPERION_RETURN_OK;
    });

    // execute command stack
    HYPERION_PASS_ERRORS(commands.Execute(), result);

    if (HasAssignedImageData()) {
        if (result) {
            // destroy staging buffer
            HYPERION_PASS_ERRORS(staging_buffer.Destroy(device), result);
        } else {
            HYPERION_IGNORE_ERRORS(staging_buffer.Destroy(device));
        }

        if (!(m_flags & IMAGE_FLAGS_KEEP_IMAGE_DATA)) {
            m_streamed_data->Unpage();
        }
    }

    return result;
}

template <>
ResourceState Image<Platform::VULKAN>::GetResourceState() const
    { return m_platform_impl.GetResourceState(); }

template <>
void Image<Platform::VULKAN>::SetResourceState(ResourceState new_state)
    { m_platform_impl.SetResourceState(new_state); }

template <>
ResourceState Image<Platform::VULKAN>::GetSubResourceState(const ImageSubResource &sub_resource) const
    { return m_platform_impl.GetSubResourceState(sub_resource); }

template <>
void Image<Platform::VULKAN>::SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state)
    { m_platform_impl.SetSubResourceState(sub_resource, new_state); }

template <>
void Image<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    ResourceState new_state
)
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_NONE;

    if (IsDepthStencil()) {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL;
    } else {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    }

    m_platform_impl.InsertBarrier(command_buffer, new_state, flags);
}

template <>
void Image<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    m_platform_impl.InsertBarrier(command_buffer, sub_resource, new_state);
}

template <>
void Image<Platform::VULKAN>::InsertSubResourceBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    m_platform_impl.InsertSubResourceBarrier(command_buffer, sub_resource, new_state);
}

template <>
Result Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src_image,
    Rect<uint32> src_rect,
    Rect<uint32> dst_rect,
    uint32 src_mip,
    uint32 dst_mip
)
{
    const auto num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint32 face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level = src_mip
        };

        const ImageSubResource dst {
            .flags = IsDepthStencil()
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
            command_buffer->GetPlatformImpl().command_buffer,
            src_image->m_platform_impl.handle,
            GetVkImageLayout(src_image->m_platform_impl.GetResourceState()),
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(src_image->GetMinFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

template <>
Result Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src_image,
    Rect<uint32> src_rect,
    Rect<uint32> dst_rect
)
{
    const uint32 num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint32 face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level = 0
        };

        const ImageSubResource dst {
            .flags = IsDepthStencil()
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
            command_buffer->GetPlatformImpl().command_buffer,
            src_image->m_platform_impl.handle,
            GetVkImageLayout(src_image->m_platform_impl.GetResourceState()),
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(src_image->GetMinFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

template <>
Result Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src
)
{
    return Blit(
        command_buffer,
        src,
        Rect<uint32> {
            .x0 = 0,
            .y0 = 0,
            .x1 = src->GetExtent().width,
            .y1 = src->GetExtent().height
        },
        Rect<uint32> {
            .x0 = 0,
            .y0 = 0,
            .x1 = m_texture_desc.extent.width,
            .y1 = m_texture_desc.extent.height
        }
    );
}

template <>
void Image<Platform::VULKAN>::CopyFromBuffer(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const GPUBuffer<Platform::VULKAN> *src_buffer
) const
{
    const auto flags = IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspect_flag_bits = 
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
    // copy from staging to image
    const auto num_faces = NumFaces();
    const auto buffer_offset_step = uint(m_size) / num_faces;

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
        region.imageExtent = VkExtent3D { m_texture_desc.extent.width, m_texture_desc.extent.height, m_texture_desc.extent.depth };

        vkCmdCopyBufferToImage(
            command_buffer->GetPlatformImpl().command_buffer,
            src_buffer->GetPlatformImpl().handle,
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            1,
            &region
        );
    }
}

template <>
void Image<Platform::VULKAN>::CopyToBuffer(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    GPUBuffer<Platform::VULKAN> *dst_buffer
) const
{
    const auto flags = IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspect_flag_bits = 
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
    // copy from staging to image
    const auto num_faces = NumFaces();
    const auto buffer_offset_step = uint(m_size) / num_faces;

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
        region.imageExtent = VkExtent3D { m_texture_desc.extent.width, m_texture_desc.extent.height, m_texture_desc.extent.depth };

        vkCmdCopyImageToBuffer(
            command_buffer->GetPlatformImpl().command_buffer,
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            dst_buffer->GetPlatformImpl().handle,
            1,
            &region
        );
    }
}

template <>
ByteBuffer Image<Platform::VULKAN>::ReadBack(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance) const
{
    StagingBuffer<Platform::VULKAN> staging_buffer;

    SingleTimeCommands<Platform::VULKAN> commands { device };

    Result result = Result { };

    if (HasAssignedImageData()) {
        HYPERION_PASS_ERRORS(staging_buffer.Create(device, m_size), result);

        if (!result) {
            return ByteBuffer(0, nullptr);
        }


        commands.Push([this, &staging_buffer](const CommandBufferRef<Platform::VULKAN> &command_buffer)
        {
            CopyToBuffer(command_buffer, &staging_buffer);

            HYPERION_RETURN_OK;
        });
    }

    // execute command stack
    HYPERION_PASS_ERRORS(commands.Execute(), result);
    
    if (result) {
        // destroy staging buffer
        HYPERION_PASS_ERRORS(staging_buffer.Destroy(device), result);
    } else {
        HYPERION_IGNORE_ERRORS(staging_buffer.Destroy(device));
    }

    ByteBuffer byte_buffer(m_size);
    staging_buffer.Read(device, m_size, byte_buffer.Data());

    return byte_buffer;
}

#pragma endregion Image

} // namespace platform
} // namespace renderer
} // namespace hyperion
