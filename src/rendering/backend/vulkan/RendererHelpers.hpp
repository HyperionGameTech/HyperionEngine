/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_HELPERS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_HELPERS_HPP

#include <core/functional/Proc.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
enum class DescriptorSetElementType : uint32;

namespace helpers {

VkIndexType ToVkIndexType(GPUElemType);
VkFormat ToVkFormat(TextureFormat);
VkFilter ToVkFilter(TextureFilterMode);
VkSamplerAddressMode ToVkSamplerAddressMode(TextureWrapMode);
VkImageAspectFlags ToVkImageAspect(TextureFormat internal_format);
VkImageType ToVkImageType(TextureType);
VkImageViewType ToVkImageViewType(TextureType type);
VkDescriptorType ToVkDescriptorType(DescriptorSetElementType type);

} // namespace helpers

namespace platform {

template <>
struct SingleTimeCommandsPlatformImpl<Platform::vulkan>
{
    SingleTimeCommands<Platform::vulkan>* self = nullptr;
    VkCommandPool pool {};
    QueueFamilyIndices family_indices {};
};

} // namespace platform

} // namespace hyperion

#endif