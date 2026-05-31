/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanHelpers.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanFence.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanFeatures.hpp>

#include <Rendering/CommandRecorder.hpp>

#include <Core/Reflection/Enum.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

extern VulkanRenderInterface RI;

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
    case TextureFormat::R8:
        return VK_FORMAT_R8_UNORM;
    case TextureFormat::RG8:
        return VK_FORMAT_R8G8_UNORM;
    case TextureFormat::RGB8:
        return VK_FORMAT_R8G8B8_UNORM;
    case TextureFormat::RGBA8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R8_SRGB:
        return VK_FORMAT_R8_SRGB;
    case TextureFormat::RG8_SRGB:
        return VK_FORMAT_R8G8_SRGB;
    case TextureFormat::RGB8_SRGB:
        return VK_FORMAT_R8G8B8_SRGB;
    case TextureFormat::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case TextureFormat::R11G11B10F:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case TextureFormat::R10G10B10A2:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case TextureFormat::R16:
        return VK_FORMAT_R16_UINT;
    case TextureFormat::RG16:
        return VK_FORMAT_R16G16_UINT;
    case TextureFormat::RGB16:
        return VK_FORMAT_R16G16B16_UINT;
    case TextureFormat::RGBA16:
        return VK_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::R32:
        return VK_FORMAT_R32_UINT;
    case TextureFormat::RG32:
        return VK_FORMAT_R32G32_UINT;
    case TextureFormat::RGB32:
        return VK_FORMAT_R32G32B32_UINT;
    case TextureFormat::RGBA32:
        return VK_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::R16F:
        return VK_FORMAT_R16_SFLOAT;
    case TextureFormat::RG16F:
        return VK_FORMAT_R16G16_SFLOAT;
    case TextureFormat::RGB16F:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case TextureFormat::RGBA16F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case TextureFormat::R32F:
        return VK_FORMAT_R32_SFLOAT;
    case TextureFormat::RG32F:
        return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RGB32F:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case TextureFormat::RGBA32F:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case TextureFormat::BGRA8:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGR8_SRGB:
        return VK_FORMAT_B8G8R8_SRGB;
    case TextureFormat::BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case TextureFormat::D16:
        return VK_FORMAT_D16_UNORM;
    case TextureFormat::D24_S8:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::D32F:
        return VK_FORMAT_D32_SFLOAT;
    case TextureFormat::D32F_S8:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
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
    case TextureType::Texture2D:
        return VK_IMAGE_TYPE_2D;
    case TextureType::Texture3D:
        return VK_IMAGE_TYPE_3D;
    case TextureType::Cubemap:
        return VK_IMAGE_TYPE_2D;
    case TextureType::Texture2DArray:
        return VK_IMAGE_TYPE_2D;
    case TextureType::CubemapArray:
        return VK_IMAGE_TYPE_2D;
    default:
        HYP_FAIL("Unhandled texture type case %d", int(type));
    }
}

VkImageViewType ToVkImageViewType(TextureType type)
{
    switch (type)
    {
    case TextureType::Texture2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case TextureType::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case TextureType::Cubemap:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case TextureType::Texture2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case TextureType::CubemapArray:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case TextureType::Max:
        return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    default:
        HYP_FAIL("Unhandled texture type case %d", int(type));
    }
}

VkDescriptorType ToVkDescriptorType(ShaderInputType type, ShaderResourceCategory category)
{
    switch (type)
    {
    case ShaderInputType::CBV:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ShaderInputType::CBV_Dynamic:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case ShaderInputType::SRV:
        switch (category)
        {
        case ShaderResourceCategory::Buffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ShaderResourceCategory::Image:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ShaderResourceCategory::AccelerationStructure:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default:
            HYP_UNREACHABLE();
        }
    case ShaderInputType::SRV_Dynamic:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case ShaderInputType::UAV:
        switch (category)
        {
        case ShaderResourceCategory::Buffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ShaderResourceCategory::Image:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default:
            HYP_UNREACHABLE();
        }
    case ShaderInputType::UAV_Dynamic:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case ShaderInputType::Sampler:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    default:
        HYP_UNREACHABLE();
    }
}

VkImageLayout GetVkImageLayout(ResourceState state,
    bool isDepthStencil, bool onlyDepth, bool onlyStencil)
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
        if (isDepthStencil)
        {
            if (onlyDepth)
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;

            if (onlyStencil)
                return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        else
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case RS_RESOLVE_DST:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case RS_DEPTH_STENCIL:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case RS_SHADER_RESOURCE:
        if (isDepthStencil)
        {
            if (onlyDepth)
                return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

            if (onlyStencil)
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;

            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        else
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

VkAccessFlags GetVkAccessMask(ResourceState state, bool isDepthStencil)
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
        if (isDepthStencil)
            return VkAccessFlagBits(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        else
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

VkPipelineStageFlags GetVkShaderStageMask(ResourceState state,
    bool isSrc, bool isDepthStencil, ShaderModuleType shaderType)
{
    switch (state)
    {
    case RS_UNDEFINED:
    case RS_PRE_INITIALIZED:
    case RS_COMMON:
        if (!isSrc)
        {
            HYP_LOG(RenderingBackend, Warning,
                "Attempt to get shader stage mask for resource state {}, but `src` was set to false. Falling back to all commands stage mask.",
                EnumToString(state));

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
        case ShaderModuleType::Vertex:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case ShaderModuleType::Pixel:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case ShaderModuleType::Compute:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case ShaderModuleType::AnyHit:
        case ShaderModuleType::ClosestHit:
        case ShaderModuleType::RayGen:
        case ShaderModuleType::Intersect:
        case ShaderModuleType::Miss:
            if (RI.GetDevice()->GetFeatures().IsRayTracingSupported())
            {
                return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            }
            else
            {
                HYP_FAIL("ERROR: Attempted to get rayTracing shader stage mask on a device that does not support rayTracing!");
            }
            break;
        case ShaderModuleType::Geometry:
            return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        case ShaderModuleType::TessControl:
            return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        case ShaderModuleType::TessEval:
            return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        case ShaderModuleType::Mesh:
            return VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV;
        case ShaderModuleType::Task:
            return VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV;
        case ShaderModuleType::None:
        {
            VkPipelineStageFlags bits = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            if (RI.GetDevice()->GetFeatures().IsRayTracingSupported())
            {
                bits |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            }

            return bits;
        }
        default:
            HYP_UNREACHABLE();
        }
    case RS_RENDER_TARGET:
        if (isDepthStencil)
            return isSrc ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        else
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case RS_DEPTH_STENCIL:
        return isSrc ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case RS_INDIRECT_ARG:
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case RS_COPY_DST:
    case RS_COPY_SRC:
    case RS_RESOLVE_DST:
    case RS_RESOLVE_SRC:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case RS_PRESENT:
        return isSrc ? (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
        HYP_UNREACHABLE();
    }
}

VkBufferUsageFlags GetVkUsageFlags(GpuBufferType type)
{
    switch (type)
    {
    case GpuBufferType::VertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::IndexBuffer:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::ConstantBuffer:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case GpuBufferType::StructuredBuffer:
    case GpuBufferType::ByteAddressBuffer:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::RWStructuredBuffer:
    case GpuBufferType::RWByteAddressBuffer:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::ReadbackBuffer:
        return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::StagingBuffer:
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case GpuBufferType::IndirectArgsBuffer:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::ShaderBindingTable:
        return VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::AccelerationStructureBuffer:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::AccelerationStructureInstanceBuffer:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::RTMeshVertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::RTMeshIndexBuffer:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::ScratchBuffer:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    default:
        return 0;
    }
}

VmaMemoryUsage GetVmaMemoryUsage(GpuBufferType type, bool cpuAccessible)
{
    switch (type)
    {
    case GpuBufferType::VertexBuffer:
        return (cpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::IndexBuffer:
        return (cpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::ConstantBuffer:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case GpuBufferType::StructuredBuffer:       // fallthrough
    case GpuBufferType::RWStructuredBuffer:     // fallthrough
    case GpuBufferType::ByteAddressBuffer:      // fallthrough
    case GpuBufferType::RWByteAddressBuffer:    // fallthrough
        return (cpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::ReadbackBuffer:
        return (cpuAccessible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    case GpuBufferType::StagingBuffer:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case GpuBufferType::IndirectArgsBuffer:
        // ignore cpuAccessible for indirect args buffer
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::ShaderBindingTable:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GpuBufferType::AccelerationStructureBuffer:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GpuBufferType::AccelerationStructureInstanceBuffer:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GpuBufferType::RTMeshVertexBuffer:
        // ignore cpuAccessible for RT mesh vertex buffer
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::RTMeshIndexBuffer:
        // ignore cpuAccessible for RT mesh index buffer
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::ScratchBuffer:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    default:
        return VMA_MEMORY_USAGE_UNKNOWN;
    }
}

VmaAllocationCreateFlags GetVkAllocationCreateFlags(GpuBufferType type, bool cpuAccessible)
{
    switch (type)
    {
    case GpuBufferType::VertexBuffer:
        return (cpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::IndexBuffer:
        return (cpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::ConstantBuffer:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::StructuredBuffer:
    case GpuBufferType::ByteAddressBuffer:
    case GpuBufferType::RWStructuredBuffer:
    case GpuBufferType::RWByteAddressBuffer:
        return (cpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::ReadbackBuffer:
        return (cpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::StagingBuffer:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::IndirectArgsBuffer:
        // ignore cpuAccessible for indirect args buffer, as it cannot be CPU accessible regardless
        return 0;
    case GpuBufferType::ShaderBindingTable:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::AccelerationStructureBuffer:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::AccelerationStructureInstanceBuffer:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::RTMeshVertexBuffer:
        // ignore cpuAccessible for RT mesh vertex buffer, as it cannot be CPU accessible regardless
        return 0;
    case GpuBufferType::RTMeshIndexBuffer:
        // ignore cpuAccessible for RT mesh index buffer, as it cannot be CPU accessible regardless
        return 0;
    case GpuBufferType::ScratchBuffer:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    default:
        HYP_FAIL("Invalid gpu buffer type for allocation create flags");
    }
}

VkImageLayout GetInitialLayout(LoadOperation loadOperation, bool isDepthStencil, bool onlyDepth, bool onlyStencil)
{
    const uint8 loadOperationIndex = loadOperation == LoadOperation::LOAD ? 1 : 0;

    return GetVkImageLayout(PreRenderResourceStates[loadOperationIndex], isDepthStencil, onlyDepth, onlyStencil);
}

VkImageLayout GetFinalLayout(RenderPassMode renderPassMode, bool isDepthStencil, bool onlyDepth, bool onlyStencil)
{
    return GetVkImageLayout(PostRenderResourceStates[uint8(renderPassMode)], isDepthStencil, onlyDepth, onlyStencil);
}

VkAttachmentLoadOp ToVkLoadOp(LoadOperation loadOperation)
{
    switch (loadOperation)
    {
    case LoadOperation::UNDEFINED:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case LoadOperation::NONE:
         // VK_ATTACHMENT_LOAD_OP_NONE-EXT is an extension and not guaranteed to be supported, so we use DONT_CARE for now
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case LoadOperation::CLEAR:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::LOAD:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    default:
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
        return VK_ATTACHMENT_STORE_OP_NONE;
    case StoreOperation::STORE:
        return VK_ATTACHMENT_STORE_OP_STORE;
    default:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

VkImageLayout GetIntermediateLayout(bool isDepthStencil, bool hasStencil, bool onlyDepth, bool onlyStencil)
{
    if (isDepthStencil)
    {
        if (onlyStencil)
            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

        if (onlyDepth)
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;

        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

VkCompareOp ToVkDepthCompareOp(DepthCompareOp compareOp)
{
    switch (compareOp)
    {
    case DCO_LESS:
        return VK_COMPARE_OP_LESS;
    case DCO_LESS_OR_EQUAL:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case DCO_GREATER:
        return VK_COMPARE_OP_GREATER;
    case DCO_GREATER_OR_EQUAL:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case DCO_EQUAL:
        return VK_COMPARE_OP_EQUAL;
    case DCO_NOT_EQUAL:
        return VK_COMPARE_OP_NOT_EQUAL;
    case DCO_ALWAYS:
        return VK_COMPARE_OP_ALWAYS;
    case DCO_NEVER:
        return VK_COMPARE_OP_NEVER;
    default:
        return VK_COMPARE_OP_LESS;
    }
}

VkAttachmentDescription ToVkAttachmentDescription(
    const AttachmentDesc& attachmentDesc,
    RenderPassMode renderPassMode)
{
    const bool isDepthStencil = TextureUtils::IsDepthFormat(attachmentDesc.format);
    const bool hasStencil = isDepthStencil && TextureUtils::HasStencilComponent(attachmentDesc.format);
    const bool onlyDepth = hasStencil && attachmentDesc.onlyDepth;
    const bool onlyStencil = hasStencil && attachmentDesc.onlyStencil;

    return VkAttachmentDescription {
        .format = ToVkFormat(attachmentDesc.format),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = !onlyStencil ? ToVkLoadOp(attachmentDesc.loadOp) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = !onlyStencil ? ToVkStoreOp(attachmentDesc.storeOp) : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = hasStencil && !onlyDepth ? ToVkLoadOp(attachmentDesc.loadOp) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = hasStencil && !onlyDepth ? ToVkStoreOp(attachmentDesc.storeOp) : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = GetInitialLayout(attachmentDesc.loadOp, isDepthStencil, onlyDepth, onlyStencil),
        .finalLayout = GetFinalLayout(renderPassMode, isDepthStencil, onlyDepth, onlyStencil)
    };
}

VkAttachmentReference ToVkAttachmentReference(uint32 index, const AttachmentDesc& attachmentDesc)
{
    const bool isDepthStencil = TextureUtils::IsDepthFormat(attachmentDesc.format);
    const bool hasStencil = isDepthStencil && TextureUtils::HasStencilComponent(attachmentDesc.format);
    const bool onlyDepth = hasStencil && attachmentDesc.onlyDepth;
    const bool onlyStencil = hasStencil && attachmentDesc.onlyStencil;

    return VkAttachmentReference {
        .attachment = index,
        .layout = GetIntermediateLayout(isDepthStencil, hasStencil, onlyDepth, onlyStencil)
    };
}

#pragma region VulkanSingleTimeCommands

RendererResult VulkanSingleTimeCommands::Execute()
{
    AssertOnThread(g_renderThread);

    CommandRecorder cr;

    for (auto& fn : m_functions)
    {
        fn(cr);
    }

    m_functions.Clear();

    VulkanFrame tempFrame;
    CheckResultOrReturn(tempFrame.Create());

    cr.Prepare(&tempFrame);

    VulkanFence fence;
    fence.Create(/* createSignalled */ false);

    VulkanCommandBuffer commandBuffer;
    commandBuffer.Create(RI.GetDevice()->GetGraphicsQueue()->commandPools[0]);

    commandBuffer.Begin();
    cr.Execute(&commandBuffer);
    commandBuffer.End();

    // Submit to the queue
    VulkanDeviceQueue* queueGraphics = RI.GetDevice()->GetGraphicsQueue();
    CheckResultOrReturn(commandBuffer.Submit(queueGraphics, &fence, nullptr, nullptr));

    fence.Wait();

    return {};
}

#pragma endregion VulkanSingleTimeCommands

} // namespace Hyperion
