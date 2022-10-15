
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <util/img/ImageUtil.hpp>
#include <system/Debug.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

Image::BaseFormat Image::GetBaseFormat(InternalFormat fmt)
{
    switch (fmt) {
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R32_:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R32F:
        return BaseFormat::TEXTURE_FORMAT_R;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16_:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F:
        return BaseFormat::TEXTURE_FORMAT_RG;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R11G11B10F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32F:
        return BaseFormat::TEXTURE_FORMAT_RGB;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_R10G10B10A2:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F:
        return BaseFormat::TEXTURE_FORMAT_RGBA;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_BGR8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGR;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGRA;
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24:
    case InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F:
        return BaseFormat::TEXTURE_FORMAT_DEPTH;
    }

    AssertThrowMsg(false, "Unhandled image format case %d", int(fmt));
}

Image::InternalFormat Image::FormatChangeNumComponents(InternalFormat fmt, UInt8 new_num_components)
{
    if (new_num_components == 0) {
        return InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE;
    }

    new_num_components = MathUtil::Clamp(new_num_components, static_cast<UInt8>(1), static_cast<UInt8>(4));

    int current_num_components = int(NumComponents(fmt));

    return InternalFormat(int(fmt) + int(new_num_components) - current_num_components);
}

UInt Image::NumComponents(InternalFormat format)
{
    return NumComponents(GetBaseFormat(format));
}

UInt Image::NumComponents(BaseFormat format)
{
    switch (format) {
    case BaseFormat::TEXTURE_FORMAT_NONE: return 0;
    case BaseFormat::TEXTURE_FORMAT_R: return 1;
    case BaseFormat::TEXTURE_FORMAT_RG: return 2;
    case BaseFormat::TEXTURE_FORMAT_RGB: return 3;
    case BaseFormat::TEXTURE_FORMAT_BGR: return 3;
    case BaseFormat::TEXTURE_FORMAT_RGBA: return 4;
    case BaseFormat::TEXTURE_FORMAT_BGRA: return 4;
    case BaseFormat::TEXTURE_FORMAT_DEPTH: return 1;
    }

    AssertThrowMsg(false, "Unhandled base image type %d", static_cast<Int>(format));
}

bool Image::IsDepthFormat(InternalFormat fmt)
{
    return IsDepthFormat(GetBaseFormat(fmt));
}

bool Image::IsDepthFormat(BaseFormat fmt)
{
    return fmt == BaseFormat::TEXTURE_FORMAT_DEPTH;
}

bool Image::IsSRGBFormat(InternalFormat fmt)
{
    return fmt >= InternalFormat::TEXTURE_INTERNAL_FORMAT_SRGB
        && fmt < InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH;
}

VkFormat Image::ToVkFormat(InternalFormat fmt)
{
    switch (fmt) {
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8:          return VK_FORMAT_R8_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8:         return VK_FORMAT_R8G8_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8:        return VK_FORMAT_R8G8B8_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8:       return VK_FORMAT_R8G8B8A8_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8_SRGB:     return VK_FORMAT_R8_SRGB;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8_SRGB:    return VK_FORMAT_R8G8_SRGB;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB:   return VK_FORMAT_R8G8B8_SRGB;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R32_:        return VK_FORMAT_R32_UINT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16_:       return VK_FORMAT_R16G16_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R11G11B10F:  return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R10G10B10A2: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R16:         return VK_FORMAT_R16_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16:        return VK_FORMAT_R16G16_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16:       return VK_FORMAT_R16G16B16_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16:      return VK_FORMAT_R16G16B16A16_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R32:         return VK_FORMAT_R32_UINT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32:        return VK_FORMAT_R32G32_UINT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32:       return VK_FORMAT_R32G32B32_UINT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32:      return VK_FORMAT_R32G32B32A32_UINT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R16F:        return VK_FORMAT_R16_SFLOAT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F:       return VK_FORMAT_R16G16_SFLOAT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16F:      return VK_FORMAT_R16G16B16_SFLOAT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F:     return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R32F:        return VK_FORMAT_R32_SFLOAT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F:       return VK_FORMAT_R32G32_SFLOAT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32F:      return VK_FORMAT_R32G32B32_SFLOAT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F:     return VK_FORMAT_R32G32B32A32_SFLOAT;
        
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8:       return VK_FORMAT_B8G8R8A8_UNORM;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGR8_SRGB:   return VK_FORMAT_B8G8R8_SRGB;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16:    return VK_FORMAT_D16_UNORM_S8_UINT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24:    return VK_FORMAT_D24_UNORM_S8_UINT;
    case Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F:   return VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    AssertThrowMsg(false, "Unhandled texture format case %d", int(fmt));
}

VkImageType Image::ToVkType(Type type)
{
    switch (type) {
    case Image::Type::TEXTURE_TYPE_2D: return VK_IMAGE_TYPE_2D;
    case Image::Type::TEXTURE_TYPE_3D: return VK_IMAGE_TYPE_3D;
    case Image::Type::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_TYPE_2D;
    }

    AssertThrowMsg(false, "Unhandled texture type case %d", int(type));
}

VkFilter Image::ToVkFilter(FilterMode filter_mode)
{
    switch (filter_mode) {
    case FilterMode::TEXTURE_FILTER_NEAREST: // fallthrough
    case FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP: return VK_FILTER_NEAREST;
    case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP: // fallthrough
    case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP: // fallthrough
    case FilterMode::TEXTURE_FILTER_LINEAR: return VK_FILTER_LINEAR;
    }

    AssertThrowMsg(false, "Unhandled texture filter mode case %d", int(filter_mode));
}

VkSamplerAddressMode Image::ToVkSamplerAddressMode(WrapMode texture_wrap_mode)
{
    switch (texture_wrap_mode) {
    case WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case WrapMode::TEXTURE_WRAP_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    AssertThrowMsg(false, "Unhandled texture wrap mode case %d", int(texture_wrap_mode));
}

Image::Image(
    Extent3D extent,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    const InternalInfo &internal_info,
    const UByte *bytes)
    : m_extent(extent),
      m_format(format),
      m_type(type),
      m_filter_mode(filter_mode),
      m_internal_info(internal_info),
      m_image(nullptr),
      m_is_blended(false),
      m_bpp(NumComponents(GetBaseFormat(format)))
{
    m_size = m_extent.width * m_extent.height * m_extent.depth * m_bpp * NumFaces();

    if (bytes != nullptr) {
        m_bytes = new UByte[m_size];
        Memory::Copy(m_bytes, bytes, m_size);
    } else {
        m_bytes = nullptr;
    }
}

Image::Image(Image &&other) noexcept
    : m_extent(other.m_extent),
      m_format(other.m_format),
      m_type(other.m_type),
      m_filter_mode(other.m_filter_mode),
      m_internal_info(other.m_internal_info),
      m_is_blended(other.m_is_blended),
      m_image(other.m_image),
      m_size(other.m_size),
      m_bpp(other.m_bpp),
      m_bytes(other.m_bytes)
{
    other.m_image = nullptr;
    other.m_bytes = nullptr;
    other.m_is_blended = false;
    other.m_extent = Extent3D { };
}

Image::~Image()
{
    AssertExit(m_image == nullptr);

    if (m_bytes != nullptr) {
        delete[] m_bytes;
    }
}

bool Image::IsDepthStencil() const
{
    return IsDepthFormat(m_format);
}

bool Image::IsSRGB() const
{
    return IsSRGBFormat(m_format);
}

void Image::SetIsSRGB(bool srgb)
{
    const bool is_srgb = IsSRGB();

    if (srgb == is_srgb) {
        return;
    }

    const auto internal_format = m_format;

    if (is_srgb) {
        m_format = InternalFormat(static_cast<Int>(internal_format) - static_cast<Int>(InternalFormat::TEXTURE_INTERNAL_FORMAT_SRGB));

        return;
    }

    const auto to_srgb_format = InternalFormat(static_cast<Int>(InternalFormat::TEXTURE_INTERNAL_FORMAT_SRGB) + static_cast<Int>(internal_format));

    if (!IsSRGBFormat(to_srgb_format)) {
        DebugLog(
            LogType::Warn,
            "No SRGB counterpart for image type (%d)\n",
            static_cast<Int>(internal_format)
        );
    }

    m_format = to_srgb_format;
}

VkFormat Image::GetImageFormat() const  { return ToVkFormat(m_format); }
VkImageType Image::GetImageType() const { return ToVkType(m_type); }

Result Image::CreateImage(
    Device *device,
    VkImageLayout initial_layout,
    VkImageCreateInfo *out_image_info
)
{
    VkFormat format = GetImageFormat();
    VkImageType image_type = GetImageType();
    VkImageCreateFlags image_create_flags = 0;
    VkFormatFeatureFlags format_features = 0;
    VkImageFormatProperties image_format_properties{};

    if (HasMipmaps()) {
        /* Mipmapped image needs linear blitting. */
        DebugLog(LogType::Debug, "Mipmapped image needs blitting support. Enabling...\n");

        format_features |= VK_FORMAT_FEATURE_BLIT_DST_BIT
                        |  VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        m_internal_info.usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (m_filter_mode) {
        case FilterMode::TEXTURE_FILTER_LINEAR: // fallthrough
        case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
            format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP:
            format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        }
    }

    if (IsBlended()) {
        DebugLog(LogType::Debug, "Image requires blending, enabling format flag...\n");

        format_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (IsTextureCube()) {
        DebugLog(LogType::Debug, "Creating cubemap , enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.\n");

        image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    auto format_support_result = device->GetFeatures().GetImageFormatProperties(
        format,
        image_type,
        m_internal_info.tiling,
        m_internal_info.usage_flags,
        image_create_flags,
        &image_format_properties
    );

    if (!format_support_result) {
        // try a series of fixes to get the image in a valid state.

        std::vector<std::pair<const char *, std::function<Result()>>> potential_fixes;

        // convert to 32bpp image
        if (m_bpp != 4) {
            potential_fixes.emplace_back(std::make_pair(
                "Convert to 32-bpp image",
                [&]() -> Result {
                    return ConvertTo32BPP(
                        device,
                        image_type,
                        image_create_flags,
                        &image_format_properties,
                        &format
                    );
                }
            ));
        }

        for (auto &fix : potential_fixes) {
            DebugLog(LogType::Debug, "Attempting fix: '%s' ...\n", fix.first);

            auto fix_result = fix.second();

            AssertContinueMsg(fix_result, "Image fix function returned an invalid result: %s\n", fix_result.message);

            // try checking format support result again
            if ((format_support_result = device->GetFeatures().GetImageFormatProperties(
                format, image_type, m_internal_info.tiling, m_internal_info.usage_flags, image_create_flags, &image_format_properties))) {
                DebugLog(LogType::Debug, "Fix '%s' successful!\n", fix.first);
                break;
            }

            DebugLog(LogType::Warn, "Fix '%s' did not change image state to valid.\n", fix.first);
        }

        HYPERION_BUBBLE_ERRORS(format_support_result);
    }

    if (format_features != 0) {
        if (!device->GetFeatures().IsSupportedFormat(format, m_internal_info.tiling, format_features)) {
            DebugLog(
                LogType::Error,
                "Device does not support the format %u with requested tiling %d and format features %d\n",
                static_cast<UInt>(format),
                static_cast<Int>(m_internal_info.tiling),
                static_cast<Int>(format_features)
            );

            HYP_BREAKPOINT;

            return Result(Result::RENDERER_ERR, "Format does not support requested features.");
        }
    }

    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const UInt32 image_family_indices[] = { qf_indices.graphics_family.value(), qf_indices.compute_family.value() };

    VkImageCreateInfo image_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.imageType = image_type;
    image_info.extent.width = m_extent.width;
    image_info.extent.height = m_extent.height;
    image_info.extent.depth = m_extent.depth;
    image_info.mipLevels = NumMipmaps();
    image_info.arrayLayers = NumFaces();
    image_info.format = format;
    image_info.tiling = m_internal_info.tiling;
    image_info.initialLayout = initial_layout;
    image_info.usage = m_internal_info.usage_flags;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = image_create_flags; // TODO: look into flags for sparse s for VCT
    image_info.pQueueFamilyIndices = image_family_indices;
    image_info.queueFamilyIndexCount = static_cast<UInt32>(std::size(image_family_indices));

    *out_image_info = image_info;

    m_image = new GPUImageMemory();

    HYPERION_BUBBLE_ERRORS(m_image->Create(device, m_size, &image_info));

    HYPERION_RETURN_OK;
}

Result Image::Create(Device *device)
{
    VkImageCreateInfo image_info;

    return CreateImage(device, VK_IMAGE_LAYOUT_UNDEFINED, &image_info);
}

Result Image::Create(Device *device, Instance *instance, ResourceState state)
{
    auto result = Result::OK;

    VkImageCreateInfo image_info;

    HYPERION_BUBBLE_ERRORS(CreateImage(device, VK_IMAGE_LAYOUT_UNDEFINED, &image_info));

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

    auto commands = instance->GetSingleTimeCommands();
    StagingBuffer staging_buffer;

    if (HasAssignedImageData())  {
        /* transition from 'undefined' layout state into one optimal for transfer */
        HYPERION_PASS_ERRORS(staging_buffer.Create(device, m_size), result);

        if (!result) {
            HYPERION_IGNORE_ERRORS(Destroy(device));

            return result;            
        }

        staging_buffer.Copy(device, m_size, m_bytes);
        // safe to delete m_bytes here?

        commands.Push([&](CommandBuffer *command_buffer) {
            m_image->InsertBarrier(
                command_buffer,
                sub_resource,
                ResourceState::COPY_DST
            );

            HYPERION_RETURN_OK;
        });

        // copy from staging to image
        const auto num_faces = NumFaces();

        UInt buffer_offset_step = static_cast<UInt>(m_size) / num_faces;

        for (UInt i = 0; i < num_faces; i++) {
            commands.Push([this, &staging_buffer, &image_info, i, buffer_offset_step](CommandBuffer *command_buffer) {
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
                    command_buffer->GetCommandBuffer(),
                    staging_buffer.buffer,
                    m_image->image,
                    GPUMemory::GetImageLayout(m_image->GetResourceState()),
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
            commands.Push([this, device](CommandBuffer *command_buffer) {
                return GenerateMipmaps(device, command_buffer);
            });
        }
    }

    /* Transition from whatever the previous layout state was to our destination state */
    commands.Push([&](CommandBuffer *command_buffer) {
        m_image->InsertBarrier(
            command_buffer,
            sub_resource,
            state
        );

        HYPERION_RETURN_OK;
    });

    // execute command stack
    HYPERION_PASS_ERRORS(commands.Execute(device), result);

    if (HasAssignedImageData()) {
        if (result) {
            // destroy staging buffer
            HYPERION_PASS_ERRORS(staging_buffer.Destroy(device), result);
        } else {
            HYPERION_IGNORE_ERRORS(staging_buffer.Destroy(device));
        }

        // delete[] m_bytes;
        // m_bytes = nullptr;
    }

    return result;
}

Result Image::Destroy(Device *device)
{
    auto result = Result::OK;

    if (m_image != nullptr) {
        HYPERION_PASS_ERRORS(m_image->Destroy(device), result);

        delete m_image;
        m_image = nullptr;
    }

    if (m_bytes) {
        delete[] m_bytes;
        m_bytes = nullptr;
    }

    return result;
}

Result Image::Blit(
    CommandBuffer *command_buffer,
    const Image *src
)
{
    return Blit(
        command_buffer,
        src,
        Rect {
            .x0 = 0,
            .y0 = 0,
            .x1 = src->GetExtent().width,
            .y1 = src->GetExtent().height
        },
        Rect {
            .x0 = 0,
            .y0 = 0,
            .x1 = m_extent.width,
            .y1 = m_extent.height
        }
    );
}

Result Image::Blit(
    CommandBuffer *command_buffer,
    const Image *src_image,
    Rect src_rect,
    Rect dst_rect
)
{
    const auto num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());
    const auto num_mipmaps = MathUtil::Min(NumMipmaps(), src_image->NumMipmaps());

    for (UInt face = 0; face < num_faces; face++) {
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
            command_buffer->GetCommandBuffer(),
            src_image->GetGPUImage()->image,
            GPUMemory::GetImageLayout(src_image->GetGPUImage()->GetResourceState()),
            this->GetGPUImage()->image,
            GPUMemory::GetImageLayout(this->GetGPUImage()->GetResourceState()),
            1, &blit,
            ToVkFilter(src_image->GetFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

Result Image::GenerateMipmaps(
    Device *device,
    CommandBuffer *command_buffer
)
{
    if (m_image == nullptr) {
        return { Result::RENDERER_ERR, "Cannot generate mipmaps on uninitialized image" };
    }

    const auto num_faces = NumFaces();
    const auto num_mipmaps = NumMipmaps();

    for (UInt32 face = 0; face < num_faces; face++) {
        for (Int32 i = 1; i < static_cast<Int32>(num_mipmaps + 1); i++) {
            const auto mip_width = static_cast<Int32>(helpers::MipmapSize(m_extent.width, i)),
                mip_height = static_cast<Int32>(helpers::MipmapSize(m_extent.height, i)),
                mip_depth = static_cast<Int32>(helpers::MipmapSize(m_extent.depth, i));

            /* Memory barrier for transfer - note that after generating the mipmaps,
                we'll still need to transfer into a layout primed for reading from shaders. */

            const ImageSubResource src {
                .flags = IsDepthStencil()
                    ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                    : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
                .base_array_layer = face,
                .base_mip_level = static_cast<UInt32>(i - 1)
            };

            const ImageSubResource dst {
                .flags = src.flags,
                .base_array_layer = src.base_array_layer,
                .base_mip_level  = static_cast<UInt32>(i)
            };
            
            m_image->InsertSubResourceBarrier(
                command_buffer,
                src,
                ResourceState::COPY_SRC
            );

            if (i == static_cast<Int32>(num_mipmaps)) {
                if (face == num_faces - 1) {
                    /* all individual subresources have been set so we mark the whole
                     * resource as being int his state */
                    m_image->SetResourceState(ResourceState::COPY_SRC);
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
                    {0, 0, 0},
                    {
                        static_cast<Int32>(helpers::MipmapSize(m_extent.width, i - 1)),
                        static_cast<Int32>(helpers::MipmapSize(m_extent.height, i - 1)),
                        static_cast<Int32>(helpers::MipmapSize(m_extent.depth, i - 1))
                    }
                },
                .dstSubresource = {
                    .aspectMask  = aspect_flag_bits,
                    .mipLevel = dst.base_mip_level,
                    .baseArrayLayer = dst.base_array_layer,
                    .layerCount = dst.num_layers
                },
                .dstOffsets = {
                    {0, 0, 0},
                    {
                        mip_width,
                        mip_height,
                        mip_depth
                    }
                }
            };

            vkCmdBlitImage(
                command_buffer->GetCommandBuffer(),
                m_image->image,
                GPUMemory::GetImageLayout(ResourceState::COPY_SRC),//m_image->GetSubResourceState(src)),
                m_image->image,
                GPUMemory::GetImageLayout(ResourceState::COPY_DST),//m_image->GetSubResourceState(dst)),
                1, &blit,
                IsDepthStencil() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR // TODO: base on filter mode
            );
        }
    }

    HYPERION_RETURN_OK;
}

void Image::CopyFromBuffer(
    CommandBuffer *command_buffer,
    const GPUBuffer *src_buffer
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
    const auto buffer_offset_step = static_cast<UInt>(m_size) / num_faces;

    for (UInt i = 0; i < num_faces; i++) {
        VkBufferImageCopy region{};
        region.bufferOffset = i * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_extent.width, m_extent.height, m_extent.depth };

        vkCmdCopyBufferToImage(
            command_buffer->GetCommandBuffer(),
            src_buffer->buffer,
            m_image->image,
            GPUMemory::GetImageLayout(m_image->GetResourceState()),
            1,
            &region
        );
    }
}

void Image::CopyToBuffer(
    CommandBuffer *command_buffer,
    GPUBuffer *dst_buffer
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
    const auto buffer_offset_step = static_cast<UInt>(m_size) / num_faces;

    for (UInt i = 0; i < num_faces; i++) {
        VkBufferImageCopy region { };
        region.bufferOffset = i * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_extent.width, m_extent.height, m_extent.depth };

        vkCmdCopyImageToBuffer(
            command_buffer->GetCommandBuffer(),
            m_image->image,
            GPUMemory::GetImageLayout(m_image->GetResourceState()),
            dst_buffer->buffer,
            1,
            &region
        );
    }
}

void Image::EnsureCapacity(SizeType size)
{
    if (HasAssignedImageData()) {
        if (GetByteSize() >= size) {
            return; // already has enough space
        }

        // resize
        auto *new_bytes = new UByte[size];
        // copy from existing
        Memory::Copy(new_bytes, m_bytes, GetByteSize());
        Memory::Set(new_bytes + GetByteSize(), 0, size - GetByteSize());

        delete[] m_bytes;
        m_bytes = new_bytes;
    } else {
        m_bytes = new UByte[size];
        Memory::Set(m_bytes, 0, size);
    }
}

Result Image::ConvertTo32BPP(
    Device *device,
    VkImageType image_type,
    VkImageCreateFlags image_create_flags,
    VkImageFormatProperties *out_image_format_properties,
    VkFormat *out_format
)
{
    constexpr UInt8 new_bpp = 4;

    const UInt num_faces = NumFaces();
    const UInt face_offset_step = static_cast<UInt>(m_size) / num_faces;

    const UInt new_size = num_faces * new_bpp * m_extent.width * m_extent.height * m_extent.depth;
    const UInt new_face_offset_step = new_size / num_faces;
    
    if (m_bytes != nullptr) {
        auto *new_bytes = new UByte[new_size];

        for (UInt i = 0; i < num_faces; i++) {
            ImageUtil::ConvertBPP(
                m_extent.width, m_extent.height, m_extent.depth,
                m_bpp, new_bpp,
                &m_bytes[i * face_offset_step],
                &new_bytes[i * new_face_offset_step]
            );
        }

        delete[] m_bytes;
        m_bytes = new_bytes;
    }

    m_format = FormatChangeNumComponents(m_format, new_bpp);
    m_bpp = new_bpp;
    m_size = new_size;

    *out_format = GetImageFormat();

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
