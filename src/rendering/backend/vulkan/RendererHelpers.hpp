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
#include <rendering/backend/RendererFence.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

VkIndexType ToVkIndexType(DatumType);
VkFormat ToVkFormat(InternalFormat);
VkImageType ToVkType(ImageType);
VkFilter ToVkFilter(FilterMode);
VkSamplerAddressMode ToVkSamplerAddressMode(WrapMode);
VkImageAspectFlags ToVkImageAspect(InternalFormat internal_format);
VkImageViewType ToVkImageViewType(ImageType type, bool is_array);

} // namespace helpers

namespace platform {

template <>
struct SingleTimeCommandsPlatformImpl<Platform::VULKAN>
{
    SingleTimeCommands<Platform::VULKAN>    *self = nullptr;
    VkCommandPool                           pool { };
    QueueFamilyIndices                      family_indices { };
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#endif