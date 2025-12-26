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

    virtual ~VulkanSingleTimeCommands() override = default;

    virtual RendererResult Execute() override;
};

template <class T>
concept VulkanStruct = requires(T a) {
    { a.sType } -> std::same_as<VkStructureType&>;
    { a.pNext } -> std::same_as<const void*&>;
};

namespace VulkanHelpers {

/*! \brief Chains pNext of pStruct to pNext of pNextStruct.
 *  If pStruct already has a pNext, the new pNextStruct is appended to the end of the chain. */
template <VulkanStruct TBaseType, VulkanStruct TNextType>
static inline void ChainNext(TBaseType& inStruct, TNextType* pNext)
{
    VkBaseOutStructure* pCurr = (VkBaseOutStructure*)&inStruct;

    while (pCurr->pNext != nullptr)
    {
        pCurr = pCurr->pNext;
    }

    pCurr->pNext = (VkBaseOutStructure*)pNext;
}

} // namespace VulkanHelpers

} // namespace Hyperion
