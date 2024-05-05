/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP

#include <vulkan/vulkan.h>

#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/containers/Array.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Framebuffer;

template <PlatformType PLATFORM>
class RenderPass;

template <>
struct GraphicsPipelinePlatformImpl<Platform::VULKAN>
{
    GraphicsPipeline<Platform::VULKAN>  *self = nullptr;

    Array<VkDynamicState>               vk_dynamic_states;

    VkViewport                          vk_viewport;
    VkRect2D                            vk_scissor;

    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32 width, uint32 height);

    void BuildVertexAttributes(
        const VertexAttributeSet &attribute_set,
        Array<VkVertexInputAttributeDescription> &out_vk_vertex_attributes,
        Array<VkVertexInputBindingDescription> &out_vk_vertex_binding_descriptions
    );

    void UpdateDynamicStates(VkCommandBuffer cmd);
};

} // namsepace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP
