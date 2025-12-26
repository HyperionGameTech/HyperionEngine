/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/RenderQueue.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/math/MathUtil.hpp>

namespace Hyperion {

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

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
        return VK_FORMAT_D16_UNORM_S8_UINT;
    case TF_DEPTH_24:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case TF_DEPTH_32F:
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

    tempFrame = GetRenderBackend()->MakeFrame(0);
    HYP_GFX_CHECK(tempFrame->Create());

    renderQueue.Prepare(tempFrame);

    tempFrame->UpdateUsedDescriptorSets();

    commandBuffer = CreateObject<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    HYP_GFX_CHECK(commandBuffer->Create(GetRenderBackend()->GetDevice()->GetGraphicsQueue()->commandPools[0]));

    HYP_GFX_CHECK(commandBuffer->Begin());

    // Execute the command list
    renderQueue.Execute(commandBuffer);

    HYP_GFX_CHECK(commandBuffer->End());

    /// \todo Refactor to use frame's fence instead, just need to make Frame able to not be presentable
    fence = CreateObject<VulkanFence>();
    HYP_GFX_CHECK(fence->Create());
    HYP_GFX_CHECK(fence->Reset());

    // Submit to the queue
    VulkanDeviceQueue* queueGraphics = GetRenderBackend()->GetDevice()->GetGraphicsQueue();

    HYP_GFX_CHECK(commandBuffer->SubmitPrimary(queueGraphics, fence, nullptr, nullptr));

    HYP_GFX_CHECK(fence->Wait());

    fence.Reset();
    commandBuffer.Reset();
    tempFrame.Reset();

    return {};
}

#pragma endregion VulkanSingleTimeCommands

} // namespace Hyperion
