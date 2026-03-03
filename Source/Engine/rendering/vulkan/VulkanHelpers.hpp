/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <vulkan/vulkan.h>

#include <Core/Types.hpp>

namespace Hyperion {

enum class ShaderInputType : uint8;
enum class ShaderModuleType : uint8;

enum class VulkanRenderPassMode : uint8
{
    RenderTarget,
    Presentation,

    Max
};

constexpr ResourceState PreRenderResourceStates[2] = {
    // CLEAR=0, LOAD=1
    RS_UNDEFINED,    // CLEAR
    RS_RENDER_TARGET // LOAD
};

constexpr ResourceState PostRenderResourceStates[uint8(VulkanRenderPassMode::Max)] = {
    RS_RENDER_TARGET,   // RenderTarget
    RS_PRESENT          // Presentation
};

VkIndexType ToVkIndexType(GpuElemType);
VkFormat ToVkFormat(TextureFormat);
VkFilter ToVkFilter(TextureFilterMode);
VkSamplerAddressMode ToVkSamplerAddressMode(TextureWrapMode);
VkImageAspectFlags ToVkImageAspect(TextureFormat);
VkImageType ToVkImageType(TextureType);
VkImageViewType ToVkImageViewType(TextureType);
VkDescriptorType ToVkDescriptorType(ShaderInputType);
VkImageLayout GetVkImageLayout(ResourceState state, bool isDepthStencil = false, bool onlyDepth = false, bool onlyStencil = false);
VkAccessFlags GetVkAccessMask(ResourceState state, bool isDepthStencil = false);
VkPipelineStageFlags GetVkShaderStageMask(ResourceState state, bool isSrc, bool isDepthStencil, ShaderModuleType shaderType = (ShaderModuleType)0);
VkBufferUsageFlags GetVkUsageFlags(GpuBufferType type);
VmaMemoryUsage GetVmaMemoryUsage(GpuBufferType type, bool requireCpuAccessible = false);
VmaAllocationCreateFlags GetVkAllocationCreateFlags(GpuBufferType type, bool requireCpuAccessible = false);
VkImageLayout GetInitialLayout(LoadOperation loadOperation, bool isDepthStencil, bool onlyDepth = false, bool onlyStencil = false);
VkImageLayout GetFinalLayout(VulkanRenderPassMode renderPassMode, bool isDepthStencil, bool onlyDepth = false, bool onlyStencil = false);
VkAttachmentLoadOp ToVkLoadOp(LoadOperation loadOperation);
VkAttachmentStoreOp ToVkStoreOp(StoreOperation storeOperation);
VkImageLayout GetIntermediateLayout(bool isDepthStencil, bool hasStencil, bool onlyDepth, bool onlyStencil);
VkBlendFactor ToVkBlendFactor(BlendModeFactor blendMode);
VkStencilOp ToVkStencilOp(StencilOp stencilOp);
VkCompareOp ToVkCompareOp(StencilCompareOp compareOp);
VkAttachmentDescription ToVkAttachmentDescription(const AttachmentDesc& attachmentDesc, VulkanRenderPassMode renderPassMode);
VkAttachmentReference ToVkAttachmentReference(uint32 index, const AttachmentDesc& attachmentDesc);

class VulkanSingleTimeCommands final : public SingleTimeCommands
{
public:
    VulkanSingleTimeCommands() = default;

    ~VulkanSingleTimeCommands() override = default;

    RendererResult Execute() override;
};

template <class T>
concept VulkanStruct = requires(T a) {
    { a.sType } -> std::convertible_to<VkStructureType&>;
    { a.pNext } -> std::convertible_to<const void*>;
};

namespace VulkanHelpers {

/*! \brief Attach \p next to the struct chain starting at \p inStruct, making it the new tail of the structure.. */
template <VulkanStruct TBaseType, VulkanStruct TNextType>
static inline void ChainNext(TBaseType& inStruct, TNextType* next)
{
    VkBaseOutStructure* current = (VkBaseOutStructure*)&inStruct;
    Assert(current != (VkBaseOutStructure*)next);

    while (current->pNext != nullptr)
    {
        // check if we'd create circular dependency
        Assert(current->pNext != (VkBaseOutStructure*)next);
        current = current->pNext;
    }

    current->pNext = (VkBaseOutStructure*)next;
}

} // namespace VulkanHelpers

} // namespace Hyperion
