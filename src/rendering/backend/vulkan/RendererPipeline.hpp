/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
    
class DescriptorPool;

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Shader;

template <>
struct PipelinePlatformImpl<Platform::VULKAN>
{
    Pipeline<Platform::VULKAN>  *self = nullptr;

    VkPipeline                  handle = VK_NULL_HANDLE;
    VkPipelineLayout            layout = VK_NULL_HANDLE;

    Array<VkDescriptorSetLayout> GetDescriptorSetLayouts() const;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
