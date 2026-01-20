/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>

#include <rendering/RenderQueue.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/math/MathUtil.hpp>

namespace Hyperion {

extern VulkanRenderBackend* g_renderBackend;

VkIndexType ToVkIndexType(GpuElemType elemType)
{
    switch (elemType)
    {
    case GET_UNSIGNED_BYTE:
        return VK_INDEX_TYPE_UINT8_EXT;
    case GET_UNSIGNED_SHORT:
        return VK_INDEX_TYPE_UINT16;
    case GET_UNSIGNED_INT:
        return VK_INDEX_TYPE_UINT32;
    default:
        HYP_FAIL("Unsupported gpu element type to vulkan index type conversion: %d", int(elemType));
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
    case TF_R11G11B10F:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case TF_R10G10B10A2:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case TF_R16:
        return VK_FORMAT_R16_UINT;
    case TF_RG16_: // fallthrough
    case TF_RG16:
        return VK_FORMAT_R16G16_UINT;
    case TF_RGB16:
        return VK_FORMAT_R16G16B16_UINT;
    case TF_RGBA16:
        return VK_FORMAT_R16G16B16A16_UINT;
    case TF_R32_: // fallthrough
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
        return VK_FORMAT_D16_UNORM;
    case TF_DEPTH_24:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case TF_DEPTH_32F:
        return VK_FORMAT_D32_SFLOAT;
    default:
        break;
    }

    HYP_FAIL("Unhandled texture format case %d", int(fmt));
}

VkFilter ToVkFilter(TextureFilterMode filterMode)
{
    switch (filterMode)
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

    HYP_FAIL("Unhandled texture filter mode case %d", int(filterMode));
}

VkSamplerAddressMode ToVkSamplerAddressMode(TextureWrapMode textureWrapMode)
{
    switch (textureWrapMode)
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
    return TextureUtils::IsDepthFormat(fmt)
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
        HYP_UNREACHABLE();
    }
}

VkImageLayout GetVkImageLayout(ResourceState state)
{
    switch (state)
    {
    case RS_UNDEFINED:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case RS_PRE_INITIALIZED:
        return VK_IMAGE_LAYOUT_PREINITIALIZED;
    case RS_COMMON:
    case RS_UNORDERED_ACCESS:
        return VK_IMAGE_LAYOUT_GENERAL;
    case RS_RENDER_TARGET:
    case RS_RESOLVE_DST:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case RS_DEPTH_STENCIL:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case RS_SHADER_RESOURCE:
    case RS_RESOLVE_SRC:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case RS_COPY_DST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case RS_COPY_SRC:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case RS_PRESENT:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default:
        HYP_FAIL("Unknown ResourceState {}!", state);
    }
}

VkAccessFlags GetVkAccessMask(ResourceState state)
{
    switch (state)
    {
    case RS_UNDEFINED:
    case RS_PRESENT:
    case RS_COMMON:
    case RS_PRE_INITIALIZED:
        return VkAccessFlagBits(0);
    case RS_VERTEX_BUFFER:
        return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case RS_CONSTANT_BUFFER:
        return VK_ACCESS_UNIFORM_READ_BIT;
    case RS_INDEX_BUFFER:
        return VK_ACCESS_INDEX_READ_BIT;
    case RS_RENDER_TARGET:
        return VkAccessFlagBits(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    case RS_UNORDERED_ACCESS:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case RS_DEPTH_STENCIL:
        return VkAccessFlagBits(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    case RS_SHADER_RESOURCE:
        return VK_ACCESS_SHADER_READ_BIT;
    case RS_INDIRECT_ARG:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case RS_COPY_DST:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case RS_COPY_SRC:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case RS_RESOLVE_DST:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case RS_RESOLVE_SRC:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    default:
        HYP_UNREACHABLE();
    }
}

VkPipelineStageFlags GetVkShaderStageMask(ResourceState state, bool src, ShaderModuleType shaderType)
{
    switch (state)
    {
    case RS_UNDEFINED:
    case RS_PRE_INITIALIZED:
    case RS_COMMON:
        if (!src)
        {
            HYP_LOG(RenderingBackend, Warning, "Attempt to get shader stage mask for resource state but `src` was set to false. Falling back to all commands.");

            return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case RS_VERTEX_BUFFER:
    case RS_INDEX_BUFFER:
        return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    case RS_UNORDERED_ACCESS:
    case RS_CONSTANT_BUFFER:
    case RS_SHADER_RESOURCE:
        switch (shaderType)
        {
        case SMT_VERTEX:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case SMT_FRAGMENT:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case SMT_COMPUTE:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case SMT_RAY_ANY_HIT:
        case SMT_RAY_CLOSEST_HIT:
        case SMT_RAY_GEN:
        case SMT_RAY_INTERSECT:
        case SMT_RAY_MISS:
            if (g_renderBackend->GetDevice()->GetFeatures().IsRayTracingSupported())
            {
                return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            }
            else
            {
                HYP_FAIL("ERROR: Attempted to get rayTracing shader stage mask on a device that does not support rayTracing!");
            }
            break;
        case SMT_GEOMETRY:
            return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        case SMT_TESS_CONTROL:
            return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        case SMT_TESS_EVAL:
            return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        case SMT_MESH:
            return VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV;
        case SMT_TASK:
            return VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV;
        case SMT_UNSET:
        {
            VkPipelineStageFlags bits = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            if (g_renderBackend->GetDevice()->GetFeatures().IsRayTracingSupported())
            {
                bits |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            }

            return bits;
        }
        default:
            HYP_UNREACHABLE();
        }
    case RS_RENDER_TARGET:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case RS_DEPTH_STENCIL:
        return src ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case RS_INDIRECT_ARG:
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case RS_COPY_DST:
    case RS_COPY_SRC:
    case RS_RESOLVE_DST:
    case RS_RESOLVE_SRC:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case RS_PRESENT:
        return src ? (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
        HYP_UNREACHABLE();
    }
}

VkBufferUsageFlags GetVkUsageFlags(GpuBufferType type)
{
    switch (type)
    {
    case GpuBufferType::MESH_VERTEX_BUFFER:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    case GpuBufferType::MESH_INDEX_BUFFER:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case GpuBufferType::CBUFF:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case GpuBufferType::SSBO:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GpuBufferType::ATOMIC_COUNTER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::STAGING_BUFFER:
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::INDIRECT_ARGS_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::SHADER_BINDING_TABLE:
        return VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::RT_MESH_VERTEX_BUFFER:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GpuBufferType::RT_MESH_INDEX_BUFFER:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GpuBufferType::SCRATCH_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    default:
        return 0;
    }
}

VmaMemoryUsage GetVmaMemoryUsage(GpuBufferType type, bool requireCpuAccessible)
{
    switch (type)
    {
    case GpuBufferType::MESH_VERTEX_BUFFER:
        return (requireCpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::MESH_INDEX_BUFFER:
        return (requireCpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::CBUFF:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GpuBufferType::SSBO:
        return (requireCpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::ATOMIC_COUNTER:
        return (requireCpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::STAGING_BUFFER:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case GpuBufferType::INDIRECT_ARGS_BUFFER:
        Assert(!requireCpuAccessible, "Indirect args buffer cannot be CPU accessible!");
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::SHADER_BINDING_TABLE:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GpuBufferType::RT_MESH_VERTEX_BUFFER:
        Assert(!requireCpuAccessible, "RT mesh vertex buffer cannot be CPU accessible!");
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::RT_MESH_INDEX_BUFFER:
        Assert(!requireCpuAccessible, "RT mesh index buffer cannot be CPU accessible!");
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::SCRATCH_BUFFER:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    default:
        return VMA_MEMORY_USAGE_UNKNOWN;
    }
}

VmaAllocationCreateFlags GetVkAllocationCreateFlags(GpuBufferType type, bool requireCpuAccessible)
{
    switch (type)
    {
    case GpuBufferType::MESH_VERTEX_BUFFER:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::MESH_INDEX_BUFFER:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::CBUFF:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::SSBO:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::ATOMIC_COUNTER:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::STAGING_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::INDIRECT_ARGS_BUFFER:
        Assert(!requireCpuAccessible, "Indirect args buffer cannot be CPU accessible!");
        return 0;
    case GpuBufferType::SHADER_BINDING_TABLE:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::RT_MESH_VERTEX_BUFFER:
        Assert(!requireCpuAccessible, "RT mesh vertex buffer cannot be CPU accessible!");
        return 0;
    case GpuBufferType::RT_MESH_INDEX_BUFFER:
        Assert(!requireCpuAccessible, "RT mesh index buffer cannot be CPU accessible!");
        return 0;
    case GpuBufferType::SCRATCH_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    default:
        HYP_FAIL("Invalid gpu buffer type for allocation create flags");
    }
}

VkImageLayout GetInitialLayout(LoadOperation loadOperation, bool isDepthAttachment)
{
    const int loadOperationIndex = loadOperation == LoadOperation::LOAD ? 1 : 0;

    return GetVkImageLayout(isDepthAttachment ? PreRenderResourceStatesDepth[loadOperationIndex] : PreRenderResourceStates[loadOperationIndex]);
}

VkImageLayout GetFinalLayout(VulkanRenderPassMode renderPassMode, bool isDepthAttachment)
{
    return GetVkImageLayout(isDepthAttachment ? PostRenderResourceStatesDepth[uint8(renderPassMode)] : PostRenderResourceStates[uint8(renderPassMode)]);
}

VkAttachmentLoadOp ToVkLoadOp(LoadOperation loadOperation)
{
    switch (loadOperation)
    {
    case LoadOperation::UNDEFINED:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case LoadOperation::NONE:
        return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
    case LoadOperation::CLEAR:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::LOAD:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    default:
        HYP_UNREACHABLE();
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp ToVkStoreOp(StoreOperation storeOperation)
{
    switch (storeOperation)
    {
    case StoreOperation::UNDEFINED:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::NONE:
        return VK_ATTACHMENT_STORE_OP_NONE_EXT;
    case StoreOperation::STORE:
        return VK_ATTACHMENT_STORE_OP_STORE;
    default:
        HYP_UNREACHABLE();
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

VkImageLayout GetIntermediateLayout(bool isDepthAttachment)
{
    return isDepthAttachment
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

VkBlendFactor ToVkBlendFactor(BlendModeFactor blendMode)
{
    switch (blendMode)
    {
    case BMF_ONE:
        return VK_BLEND_FACTOR_ONE;
    case BMF_ZERO:
        return VK_BLEND_FACTOR_ZERO;
    case BMF_SRC_COLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case BMF_SRC_ALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BMF_DST_COLOR:
        return VK_BLEND_FACTOR_DST_COLOR;
    case BMF_DST_ALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case BMF_ONE_MINUS_SRC_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BMF_ONE_MINUS_SRC_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BMF_ONE_MINUS_DST_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BMF_ONE_MINUS_DST_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:
        return VK_BLEND_FACTOR_ONE;
    }
}

VkStencilOp ToVkStencilOp(StencilOp stencilOp)
{
    switch (stencilOp)
    {
    case SO_KEEP:
        return VK_STENCIL_OP_KEEP;
    case SO_ZERO:
        return VK_STENCIL_OP_ZERO;
    case SO_REPLACE:
        return VK_STENCIL_OP_REPLACE;
    case SO_INCREMENT:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case SO_DECREMENT:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    default:
        return VK_STENCIL_OP_KEEP;
    }
}

VkCompareOp ToVkCompareOp(StencilCompareOp compareOp)
{
    switch (compareOp)
    {
    case SCO_ALWAYS:
        return VK_COMPARE_OP_ALWAYS;
    case SCO_NEVER:
        return VK_COMPARE_OP_NEVER;
    case SCO_EQUAL:
        return VK_COMPARE_OP_EQUAL;
    case SCO_NOT_EQUAL:
        return VK_COMPARE_OP_NOT_EQUAL;
    default:
        return VK_COMPARE_OP_ALWAYS;
    }
}

#pragma region VulkanSingleTimeCommands

RendererResult VulkanSingleTimeCommands::Execute()
{
    AssertOnThread(g_renderThread);

    VulkanFrameRef tempFrame;
    VulkanCommandBufferRef commandBuffer;
    VulkanFenceRef fence;

    RenderQueue renderQueue;

    for (auto& fn : m_functions)
    {
        fn(renderQueue);
    }

    m_functions.Clear();

    tempFrame = g_renderBackend->MakeFrame(0);
    CheckResultOrReturn(tempFrame->Create());

    renderQueue.Prepare(tempFrame);

    tempFrame->UpdateUsedDescriptorSets();

    commandBuffer = CreateObject<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CheckResultOrReturn(commandBuffer->Create(g_renderBackend->GetDevice()->GetGraphicsQueue()->commandPools[0]));

    CheckResultOrReturn(commandBuffer->Begin());

    // Execute the command list
    renderQueue.Execute(commandBuffer);

    CheckResultOrReturn(commandBuffer->End());

    /// \todo Refactor to use frame's fence instead, just need to make Frame able to not be presentable
    fence = CreateObject<VulkanFence>();
    CheckResultOrReturn(fence->Create());
    CheckResultOrReturn(fence->Reset());

    // Submit to the queue
    VulkanDeviceQueue* queueGraphics = g_renderBackend->GetDevice()->GetGraphicsQueue();

    CheckResultOrReturn(commandBuffer->SubmitPrimary(queueGraphics, fence, nullptr, nullptr));

    CheckResultOrReturn(fence->Wait());

    fence.Reset();
    commandBuffer.Reset();
    tempFrame.Reset();

    return {};
}

#pragma endregion VulkanSingleTimeCommands

} // namespace Hyperion
