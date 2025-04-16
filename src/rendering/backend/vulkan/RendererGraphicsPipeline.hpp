/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP

#include <vulkan/vulkan.h>

#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <rendering/backend/vulkan/RendererPipeline.hpp>
#include <rendering/backend/vulkan/RendererBuffer.hpp>
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>

#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/containers/Array.hpp>

#include <core/math/Vector2.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

class VulkanFramebuffer;
using VulkanFramebufferRef = RenderObjectHandle_Strong<VulkanFramebuffer>;
using VulkanFramebufferWeakRef = RenderObjectHandle_Weak<VulkanFramebuffer>;

class VulkanRenderPass;
using VulkanRenderPassRef = RenderObjectHandle_Strong<VulkanRenderPass>;
using VulkanRenderPassWeakRef = RenderObjectHandle_Weak<VulkanRenderPass>;

class VulkanGraphicsPipeline final : public GraphicsPipelineBase, public VulkanPipelineBase
{
public:
    HYP_API VulkanGraphicsPipeline();
    HYP_API VulkanGraphicsPipeline(const VulkanShaderRef &shader, const VulkanDescriptorTableRef &descriptor_table);
    HYP_API ~VulkanGraphicsPipeline();

    HYP_FORCE_INLINE const VulkanRenderPassRef &GetRenderPass() const
        { return m_render_pass; }

    HYP_API void SetRenderPass(const VulkanRenderPassRef &render_pass);

    HYP_FORCE_INLINE const Array<VulkanFramebufferRef> &GetFramebuffers() const
        { return m_framebuffers; }

    HYP_API void SetFramebuffers(const Array<VulkanFramebufferRef> &framebuffers);

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;
    
    HYP_API virtual void Bind(CommandBufferBase *cmd) override;
    HYP_API virtual void Bind(CommandBufferBase *cmd, Vec2i viewport_offset, Vec2i viewport_extent) override;

    HYP_API virtual bool MatchesSignature(
        const ShaderBase *shader,
        const DescriptorTableBase *descriptor_table,
        const Array<const FramebufferBase *> &framebuffers,
        const RenderableAttributeSet &attributes
    ) const override;

    HYP_API virtual void SetPushConstants(const void *data, SizeType size) override;

private:
    RendererResult Rebuild();

    void BuildVertexAttributes(
        const VertexAttributeSet &attribute_set,
        Array<VkVertexInputAttributeDescription> &out_vk_vertex_attributes,
        Array<VkVertexInputBindingDescription> &out_vk_vertex_binding_descriptions
    );

    void UpdateViewport(VulkanCommandBuffer *command_buffer, const Viewport &viewport);

    VulkanRenderPassRef         m_render_pass;
    Array<VulkanFramebufferRef> m_framebuffers;
    Viewport                    m_viewport;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_GRAPHICS_PIPELINE_HPP
