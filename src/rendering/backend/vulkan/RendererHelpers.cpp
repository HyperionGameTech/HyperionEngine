/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {

namespace platform {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

} // namespace platform

namespace helpers {

VkIndexType ToVkIndexType(DatumType datum_type)
{
    switch (datum_type) {
    case DatumType::UNSIGNED_BYTE:
        return VK_INDEX_TYPE_UINT8_EXT;
    case DatumType::UNSIGNED_SHORT:
        return VK_INDEX_TYPE_UINT16;
    case DatumType::UNSIGNED_INT:
        return VK_INDEX_TYPE_UINT32;
    default:
        AssertThrowMsg(0, "Unsupported datum type to vulkan index type conversion: %d", int(datum_type));
    }
}

VkFormat ToVkFormat(InternalFormat fmt)
{
    switch (fmt) {
    case InternalFormat::R8:          return VK_FORMAT_R8_UNORM;
    case InternalFormat::RG8:         return VK_FORMAT_R8G8_UNORM;
    case InternalFormat::RGB8:        return VK_FORMAT_R8G8B8_UNORM;
    case InternalFormat::RGBA8:       return VK_FORMAT_R8G8B8A8_UNORM;
    case InternalFormat::R8_SRGB:     return VK_FORMAT_R8_SRGB;
    case InternalFormat::RG8_SRGB:    return VK_FORMAT_R8G8_SRGB;
    case InternalFormat::RGB8_SRGB:   return VK_FORMAT_R8G8B8_SRGB;
    case InternalFormat::RGBA8_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;
    case InternalFormat::R32_:        return VK_FORMAT_R32_UINT;
    case InternalFormat::RG16_:       return VK_FORMAT_R16G16_UNORM;
    case InternalFormat::R11G11B10F:  return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case InternalFormat::R10G10B10A2: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case InternalFormat::R16:         return VK_FORMAT_R16_UNORM;
    case InternalFormat::RG16:        return VK_FORMAT_R16G16_UNORM;
    case InternalFormat::RGB16:       return VK_FORMAT_R16G16B16_UNORM;
    case InternalFormat::RGBA16:      return VK_FORMAT_R16G16B16A16_UNORM;
    case InternalFormat::R32:         return VK_FORMAT_R32_UINT;
    case InternalFormat::RG32:        return VK_FORMAT_R32G32_UINT;
    case InternalFormat::RGB32:       return VK_FORMAT_R32G32B32_UINT;
    case InternalFormat::RGBA32:      return VK_FORMAT_R32G32B32A32_UINT;
    case InternalFormat::R16F:        return VK_FORMAT_R16_SFLOAT;
    case InternalFormat::RG16F:       return VK_FORMAT_R16G16_SFLOAT;
    case InternalFormat::RGB16F:      return VK_FORMAT_R16G16B16_SFLOAT;
    case InternalFormat::RGBA16F:     return VK_FORMAT_R16G16B16A16_SFLOAT;
    case InternalFormat::R32F:        return VK_FORMAT_R32_SFLOAT;
    case InternalFormat::RG32F:       return VK_FORMAT_R32G32_SFLOAT;
    case InternalFormat::RGB32F:      return VK_FORMAT_R32G32B32_SFLOAT;
    case InternalFormat::RGBA32F:     return VK_FORMAT_R32G32B32A32_SFLOAT;
        
    case InternalFormat::BGRA8:       return VK_FORMAT_B8G8R8A8_UNORM;
    case InternalFormat::BGR8_SRGB:   return VK_FORMAT_B8G8R8_SRGB;
    case InternalFormat::BGRA8_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
    case InternalFormat::DEPTH_16:    return VK_FORMAT_D16_UNORM_S8_UINT;
    case InternalFormat::DEPTH_24:    return VK_FORMAT_D24_UNORM_S8_UINT;
    case InternalFormat::DEPTH_32F:   return VK_FORMAT_D32_SFLOAT_S8_UINT;
    default: break;
    }

    AssertThrowMsg(false, "Unhandled texture format case %d", int(fmt));
}

VkImageType ToVkType(ImageType type)
{
    switch (type) {
    case ImageType::TEXTURE_TYPE_2D: return VK_IMAGE_TYPE_2D;
    case ImageType::TEXTURE_TYPE_3D: return VK_IMAGE_TYPE_3D;
    case ImageType::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_TYPE_2D;
    }

    AssertThrowMsg(false, "Unhandled texture type case %d", int(type));
}

VkFilter ToVkFilter(FilterMode filter_mode)
{
    switch (filter_mode) {
    case FilterMode::TEXTURE_FILTER_NEAREST: // fallthrough
    case FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP:
        return VK_FILTER_NEAREST;
    case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP: // fallthrough
    case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP: // fallthrough
    case FilterMode::TEXTURE_FILTER_LINEAR:
        return VK_FILTER_LINEAR;
    default:
        break;
    }

    AssertThrowMsg(false, "Unhandled texture filter mode case %d", int(filter_mode));
}

VkSamplerAddressMode ToVkSamplerAddressMode(WrapMode texture_wrap_mode)
{
    switch (texture_wrap_mode) {
    case WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case WrapMode::TEXTURE_WRAP_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    default:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkImageAspectFlags ToVkImageAspect(InternalFormat internal_format)
{
    return IsDepthFormat(internal_format)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageViewType ToVkImageViewType(ImageType type, bool is_array)
{
    if (is_array) {
        switch (type) {
        case ImageType::TEXTURE_TYPE_2D:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case ImageType::TEXTURE_TYPE_CUBEMAP:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        default:
            HYP_FAIL("Unhandled texture type case %d", int(type));
        }
    }

    switch (type) {
    case ImageType::TEXTURE_TYPE_2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case ImageType::TEXTURE_TYPE_3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case ImageType::TEXTURE_TYPE_CUBEMAP:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    default:
        HYP_FAIL("Unhandled texture type case %d", int(type));
    }
}

VkDescriptorType ToVkDescriptorType(DescriptorSetElementType type)
{
    switch (type) {
    case DescriptorSetElementType::UNIFORM_BUFFER:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case DescriptorSetElementType::STORAGE_BUFFER:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case DescriptorSetElementType::IMAGE:                  return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorSetElementType::SAMPLER:                return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorSetElementType::IMAGE_STORAGE:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorSetElementType::TLAS:                   return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type for Vulkan");
    }
}

} // namespace helpers

namespace platform {

#pragma region SingleTimeCommands

template <>
SingleTimeCommands<Platform::VULKAN>::SingleTimeCommands()
    : m_platform_impl { this }
{
}

template <>
SingleTimeCommands<Platform::VULKAN>::~SingleTimeCommands()
{
}

template <>
RendererResult SingleTimeCommands<Platform::VULKAN>::Execute()
{
    RHICommandList command_list;

    for (auto &fn : m_functions) {
        fn(command_list);
    }

    m_functions.Clear();

    RendererResult result;

    FenceRef<Platform::VULKAN> fence = MakeRenderObject<Fence<Platform::VULKAN>>();

    CommandBufferRef<Platform::VULKAN> command_buffer = MakeRenderObject<CommandBuffer<Platform::VULKAN>>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
    command_buffer->GetPlatformImpl().command_pool = GetRenderingAPI()->GetDevice()->GetGraphicsQueue().command_pools[0];
    HYPERION_BUBBLE_ERRORS(command_buffer->Create());

    HYPERION_BUBBLE_ERRORS(command_buffer->Begin());

    // Execute the command list
    command_list.Execute(command_buffer);

    HYPERION_PASS_ERRORS(command_buffer->End(), result);

    HYPERION_PASS_ERRORS(fence->Create(), result);
    HYPERION_PASS_ERRORS(fence->Reset(), result);

    // Submit to the queue
    DeviceQueue<Platform::VULKAN> &queue_graphics = GetRenderingAPI()->GetDevice()->GetGraphicsQueue();

    HYPERION_PASS_ERRORS(command_buffer->SubmitPrimary(&queue_graphics, fence, nullptr), result);
    
    HYPERION_PASS_ERRORS(fence->WaitForGPU(), result);
    HYPERION_PASS_ERRORS(fence->Destroy(), result);

    HYPERION_PASS_ERRORS(command_buffer->Destroy(), result);

    return result;
}

#pragma endregion SingleTimeCommands

} // namespace platform

} // namespace helpers
} // namespace hyperion