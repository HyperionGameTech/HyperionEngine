/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP
#define HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <core/math/Vertex.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class RenderableAttributeSet;

namespace renderer {

class GraphicsPipelineBase : public RenderObject<GraphicsPipelineBase>
{
public:
    HYP_API ~GraphicsPipelineBase() override = default;

    HYP_FORCE_INLINE const VertexAttributeSet& GetVertexAttributes() const
    {
        return m_vertex_attributes;
    }

    HYP_FORCE_INLINE void SetVertexAttributes(const VertexAttributeSet& vertex_attributes)
    {
        m_vertex_attributes = vertex_attributes;
    }

    HYP_FORCE_INLINE Topology GetTopology() const
    {
        return m_topology;
    }

    HYP_FORCE_INLINE void SetTopology(Topology topology)
    {
        m_topology = topology;
    }

    HYP_FORCE_INLINE FaceCullMode GetCullMode() const
    {
        return m_face_cull_mode;
    }

    HYP_FORCE_INLINE void SetCullMode(FaceCullMode face_cull_mode)
    {
        m_face_cull_mode = face_cull_mode;
    }

    HYP_FORCE_INLINE FillMode GetFillMode() const
    {
        return m_fill_mode;
    }

    HYP_FORCE_INLINE void SetFillMode(FillMode fill_mode)
    {
        m_fill_mode = fill_mode;
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blend_function;
    }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction& blend_function)
    {
        m_blend_function = blend_function;
    }

    HYP_FORCE_INLINE const StencilFunction& GetStencilFunction() const
    {
        return m_stencil_function;
    }

    HYP_FORCE_INLINE void SetStencilFunction(const StencilFunction& stencil_function)
    {
        m_stencil_function = stencil_function;
    }

    HYP_FORCE_INLINE bool GetDepthTest() const
    {
        return m_depth_test;
    }

    HYP_FORCE_INLINE void SetDepthTest(bool depth_test)
    {
        m_depth_test = depth_test;
    }

    HYP_FORCE_INLINE bool GetDepthWrite() const
    {
        return m_depth_write;
    }

    HYP_FORCE_INLINE void SetDepthWrite(bool depth_write)
    {
        m_depth_write = depth_write;
    }

    HYP_FORCE_INLINE const DescriptorTableRef& GetDescriptorTable() const
    {
        return m_descriptor_table;
    }

    HYP_FORCE_INLINE void SetDescriptorTable(const DescriptorTableRef& descriptor_table)
    {
        m_descriptor_table = descriptor_table;
    }

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    HYP_FORCE_INLINE void SetShader(const ShaderRef& shader)
    {
        m_shader = shader;
    }

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual void Bind(CommandBufferBase* command_buffer) = 0;
    HYP_API virtual void Bind(CommandBufferBase* command_buffer, Vec2i viewport_offset, Vec2u viewport_extent) = 0;

    HYP_API virtual bool MatchesSignature(
        const ShaderBase* shader,
        const DescriptorTableBase* descriptor_table,
        const Array<const FramebufferBase*>& framebuffers,
        const RenderableAttributeSet& attributes) const = 0;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED HYP_API virtual void SetPushConstants(const void* data, SizeType size) = 0;

protected:
    GraphicsPipelineBase() = default;

    GraphicsPipelineBase(const ShaderRef& shader, const DescriptorTableRef& descriptor_table)
        : m_shader(shader),
          m_descriptor_table(descriptor_table)
    {
    }

    VertexAttributeSet m_vertex_attributes;

    Topology m_topology = Topology::TRIANGLES;
    FaceCullMode m_face_cull_mode = FaceCullMode::BACK;
    FillMode m_fill_mode = FillMode::FILL;
    BlendFunction m_blend_function = BlendFunction::None();

    StencilFunction m_stencil_function;

    bool m_depth_test = true;
    bool m_depth_write = true;

    ShaderRef m_shader;
    DescriptorTableRef m_descriptor_table;
};

} // namespace renderer
} // namespace hyperion

#endif
