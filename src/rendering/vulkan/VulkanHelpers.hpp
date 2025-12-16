/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <vulkan/vulkan.h>

#include <core/Types.hpp>

namespace hyperion {

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

} // namespace hyperion
