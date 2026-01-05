/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include "vulkan/vulkan_core.h"
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <vulkan/vulkan.h>

#include <core/Types.hpp>

namespace Hyperion {

enum class DescriptorSetElementType : uint32;

VkIndexType ToVkIndexType(GpuElemType);
VkFormat ToVkFormat(TextureFormat);
VkFilter ToVkFilter(TextureFilterMode);
VkSamplerAddressMode ToVkSamplerAddressMode(TextureWrapMode);
VkImageAspectFlags ToVkImageAspect(TextureFormat);
VkImageType ToVkImageType(TextureType);
VkImageViewType ToVkImageViewType(TextureType);
VkDescriptorType ToVkDescriptorType(DescriptorSetElementType);

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
    HYP_GFX_ASSERT(current != (VkBaseOutStructure*)next);

    while (current->pNext != nullptr)
    {
        // check if we'd create circular dependency
        HYP_GFX_ASSERT(current->pNext != (VkBaseOutStructure*)next);
        current = current->pNext;
    }

    current->pNext = (VkBaseOutStructure*)next;
}

} // namespace VulkanHelpers

} // namespace Hyperion
