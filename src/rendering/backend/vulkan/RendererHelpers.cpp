/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererHelpers.hpp>

#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

extern IRenderBackend* g_render_backend;

namespace platform {

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_render_backend);
}

} // namespace platform

namespace helpers {

VkIndexType ToVkIndexType(GpuElemType elem_type)
{
    switch (elem_type)
    {
    case GET_UNSIGNED_BYTE:
        return VK_INDEX_TYPE_UINT8_EXT;
    case GET_UNSIGNED_SHORT:
        return VK_INDEX_TYPE_UINT16;
    case GET_UNSIGNED_INT:
        return VK_INDEX_TYPE_UINT32;
    default:
        HYP_FAIL("Unsupported gpu element type to vulkan index type conversion: %d", int(elem_type));
    }
}

VkFormat ToVkFormat(TextureFormat fmt)
{
    switch (fmt)
    {
    case TF_R8:
        return VK_FORMAT_R8_UNORM;
    case TF_RG8:
        return VK_FORMAT_R8G8_UNORM;
    case TF_RGB8:
        return VK_FORMAT_R8G8B8_UNORM;
    case TF_RGBA8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case TF_R8_SRGB:
        return VK_FORMAT_R8_SRGB;
    case TF_RG8_SRGB:
        return VK_FORMAT_R8G8_SRGB;
    case TF_RGB8_SRGB:
        return VK_FORMAT_R8G8B8_SRGB;
    case TF_RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case TF_R32_:
        return VK_FORMAT_R32_UINT;
    case TF_RG16_:
        return VK_FORMAT_R16G16_UNORM;
    case TF_R11G11B10F:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case TF_R10G10B10A2:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case TF_R16:
        return VK_FORMAT_R16_UNORM;
    case TF_RG16:
        return VK_FORMAT_R16G16_UNORM;
    case TF_RGB16:
        return VK_FORMAT_R16G16B16_UNORM;
    case TF_RGBA16:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case TF_R32:
        return VK_FORMAT_R32_UINT;
    case TF_RG32:
        return VK_FORMAT_R32G32_UINT;
    case TF_RGB32:
        return VK_FORMAT_R32G32B32_UINT;
    case TF_RGBA32:
        return VK_FORMAT_R32G32B32A32_UINT;
    case TF_R16F:
        return VK_FORMAT_R16_SFLOAT;
    case TF_RG16F:
        return VK_FORMAT_R16G16_SFLOAT;
    case TF_RGB16F:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case TF_RGBA16F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case TF_R32F:
        return VK_FORMAT_R32_SFLOAT;
    case TF_RG32F:
        return VK_FORMAT_R32G32_SFLOAT;
    case TF_RGB32F:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case TF_RGBA32F:
        return VK_FORMAT_R32G32B32A32_SFLOAT;

    case TF_BGRA8:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case TF_BGR8_SRGB:
        return VK_FORMAT_B8G8R8_SRGB;
    case TF_BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case TF_DEPTH_16:
        return VK_FORMAT_D16_UNORM_S8_UINT;
    case TF_DEPTH_24:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case TF_DEPTH_32F:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    default:
        break;
    }

    AssertThrowMsg(false, "Unhandled texture format case %d", int(fmt));
}

VkFilter ToVkFilter(TextureFilterMode filter_mode)
{
    switch (filter_mode)
    {
    case TFM_NEAREST: // fallthrough
    case TFM_NEAREST_MIPMAP:
        return VK_FILTER_NEAREST;
    case TFM_MINMAX_MIPMAP: // fallthrough
    case TFM_LINEAR_MIPMAP: // fallthrough
    case TFM_LINEAR:
        return VK_FILTER_LINEAR;
    default:
        break;
    }

    AssertThrowMsg(false, "Unhandled texture filter mode case %d", int(filter_mode));
}

VkSamplerAddressMode ToVkSamplerAddressMode(TextureWrapMode texture_wrap_mode)
{
    switch (texture_wrap_mode)
    {
    case TWM_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case TWM_CLAMP_TO_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case TWM_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    default:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkImageAspectFlags ToVkImageAspect(TextureFormat fmt)
{
    return IsDepthFormat(fmt)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageType ToVkImageType(TextureType type)
{
    switch (type)
    {
    case TT_TEX2D:
        return VK_IMAGE_TYPE_2D;
    case TT_TEX3D:
        return VK_IMAGE_TYPE_3D;
    case TT_CUBEMAP:
        return VK_IMAGE_TYPE_2D;
    case TT_TEX2D_ARRAY:
        return VK_IMAGE_TYPE_2D;
    case TT_CUBEMAP_ARRAY:
        return VK_IMAGE_TYPE_2D;
    default:
        HYP_FAIL("Unhandled texture type case %d", int(type));
    }
}

VkImageViewType ToVkImageViewType(TextureType type)
{
    switch (type)
    {
    case TT_TEX2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case TT_TEX3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case TT_CUBEMAP:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case TT_TEX2D_ARRAY:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case TT_CUBEMAP_ARRAY:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    default:
        HYP_FAIL("Unhandled texture type case %d", int(type));
    }
}

VkDescriptorType ToVkDescriptorType(DescriptorSetElementType type)
{
    switch (type)
    {
    case DescriptorSetElementType::UNIFORM_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case DescriptorSetElementType::SSBO:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case DescriptorSetElementType::IMAGE:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorSetElementType::SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorSetElementType::IMAGE_STORAGE:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorSetElementType::TLAS:
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type for Vulkan");
    }
}

} // namespace helpers

namespace platform {

#pragma region SingleTimeCommands

template <>
SingleTimeCommands<Platform::vulkan>::SingleTimeCommands()
    : m_platform_impl { this }
{
}

template <>
SingleTimeCommands<Platform::vulkan>::~SingleTimeCommands()
{
}

template <>
RendererResult SingleTimeCommands<Platform::vulkan>::Execute()
{
    CmdList command_list;

    for (auto& fn : m_functions)
    {
        fn(command_list);
    }

    m_functions.Clear();

    RendererResult result;

    VulkanFrameRef temp_frame = VulkanFrameRef(GetRenderBackend()->MakeFrame(0));
    HYPERION_BUBBLE_ERRORS(temp_frame->Create());

    command_list.Prepare(temp_frame);

    temp_frame->UpdateUsedDescriptorSets();

    VulkanCommandBufferRef command_buffer = MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
    HYPERION_BUBBLE_ERRORS(command_buffer->Create(GetRenderBackend()->GetDevice()->GetGraphicsQueue().command_pools[0]));

    HYPERION_BUBBLE_ERRORS(command_buffer->Begin());

    // Execute the command list
    command_list.Execute(command_buffer);

    HYPERION_PASS_ERRORS(command_buffer->End(), result);

    // @TODO Refactor to use frame's fence instead, just need to make Frame able to not be presentable
    VulkanFenceRef fence = MakeRenderObject<VulkanFence>();
    HYPERION_PASS_ERRORS(fence->Create(), result);
    HYPERION_PASS_ERRORS(fence->Reset(), result);

    // Submit to the queue
    VulkanDeviceQueue& queue_graphics = GetRenderBackend()->GetDevice()->GetGraphicsQueue();

    HYPERION_PASS_ERRORS(command_buffer->SubmitPrimary(&queue_graphics, fence, nullptr), result);

    HYPERION_PASS_ERRORS(fence->WaitForGPU(), result);
    HYPERION_PASS_ERRORS(fence->Destroy(), result);

    HYPERION_PASS_ERRORS(command_buffer->Destroy(), result);

    HYPERION_PASS_ERRORS(temp_frame->Destroy(), result);

    return result;
}

#pragma endregion SingleTimeCommands

} // namespace platform

} // namespace hyperion