/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <core/math/Vertex.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class RenderableAttributeSet;
struct DescriptorTableDeclaration;

class GraphicsPipelineBase : public RenderObject<GraphicsPipelineBase>
{
public:
    virtual ~GraphicsPipelineBase() override;

    HYP_FORCE_INLINE const VertexAttributeSet& GetVertexAttributes() const
    {
        return m_vertexAttributes;
    }

    HYP_FORCE_INLINE void SetVertexAttributes(const VertexAttributeSet& vertexAttributes)
    {
        m_vertexAttributes = vertexAttributes;
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
        return m_faceCullMode;
    }

    HYP_FORCE_INLINE void SetCullMode(FaceCullMode faceCullMode)
    {
        m_faceCullMode = faceCullMode;
    }

    HYP_FORCE_INLINE FillMode GetFillMode() const
    {
        return m_fillMode;
    }

    HYP_FORCE_INLINE void SetFillMode(FillMode fillMode)
    {
        m_fillMode = fillMode;
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blendFunction;
    }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction& blendFunction)
    {
        m_blendFunction = blendFunction;
    }

    HYP_FORCE_INLINE const StencilFunction& GetStencilFunction() const
    {
        return m_stencilFunction;
    }

    HYP_FORCE_INLINE void SetStencilFunction(const StencilFunction& stencilFunction)
    {
        m_stencilFunction = stencilFunction;
    }

    HYP_FORCE_INLINE bool GetDepthTest() const
    {
        return m_depthTest;
    }

    HYP_FORCE_INLINE void SetDepthTest(bool depthTest)
    {
        m_depthTest = depthTest;
    }

    HYP_FORCE_INLINE bool GetDepthWrite() const
    {
        return m_depthWrite;
    }

    HYP_FORCE_INLINE void SetDepthWrite(bool depthWrite)
    {
        m_depthWrite = depthWrite;
    }

    HYP_FORCE_INLINE const DescriptorTableRef& GetDescriptorTable() const
    {
        return m_descriptorTable;
    }

    void SetDescriptorTable(const DescriptorTableRef& descriptorTable);

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    void SetShader(const ShaderRef& shader);

    HYP_FORCE_INLINE const Array<FramebufferRef>& GetFramebuffers() const
    {
        return m_framebuffers;
    }

    void SetFramebuffers(const Array<FramebufferRef>& framebuffers);
    
    virtual bool IsCreated() const = 0;

    virtual RendererResult Create();
    virtual RendererResult Destroy();

    virtual void Bind(CommandBufferBase* commandBuffer) = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, Vec2i viewportOffset, Vec2u viewportExtent) = 0;

    virtual bool MatchesSignature(
        const ShaderBase* shader,
        const DescriptorTableDeclaration& descriptorTableDecl,
        const Array<const FramebufferBase*>& framebuffers,
        const RenderableAttributeSet& attributes) const;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED virtual void SetPushConstants(const void* data, SizeType size) = 0;

    uint32 lastFrame = uint32(-1);

protected:
    GraphicsPipelineBase() = default;

    GraphicsPipelineBase(const ShaderRef& shader, const DescriptorTableRef& descriptorTable)
        : m_shader(shader),
          m_descriptorTable(descriptorTable)
    {
    }

    virtual RendererResult Rebuild() = 0;

    VertexAttributeSet m_vertexAttributes;

    Topology m_topology = TOP_TRIANGLES;
    FaceCullMode m_faceCullMode = FCM_BACK;
    FillMode m_fillMode = FM_FILL;
    BlendFunction m_blendFunction = BlendFunction::None();

    StencilFunction m_stencilFunction;

    bool m_depthTest = true;
    bool m_depthWrite = true;

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;
    Array<FramebufferRef> m_framebuffers;
};

} // namespace hyperion
