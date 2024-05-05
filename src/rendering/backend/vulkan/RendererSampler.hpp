/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef RENDERER_SAMPLER_HPP
#define RENDERER_SAMPLER_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;
    
template <PlatformType PLATFORM>
class ImageView;

template <>
struct SamplerPlatformImpl<Platform::VULKAN>
{
    Sampler<Platform::VULKAN>   *self = nullptr;

    VkSampler                   handle = VK_NULL_HANDLE;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif