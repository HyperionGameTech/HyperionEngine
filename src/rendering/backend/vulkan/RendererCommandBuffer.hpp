/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Frame;

template <PlatformType PLATFORM>
class Pipeline;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

template <>
struct CommandBufferPlatformImpl<Platform::VULKAN>
{
    CommandBuffer<Platform::VULKAN> *self = nullptr;
    VkCommandBuffer                 command_buffer = VK_NULL_HANDLE;
    VkCommandPool                   command_pool = VK_NULL_HANDLE;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif