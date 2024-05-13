/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP
#define HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererPipeline.hpp>

#include <math/Vertex.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
struct GraphicsPipelinePlatformImpl;

template <PlatformType PLATFORM>
class GraphicsPipeline : public Pipeline<PLATFORM>
{
public:
    HYP_API GraphicsPipeline();
    HYP_API GraphicsPipeline(const ShaderRef<PLATFORM> &shader, const DescriptorTableRef<PLATFORM> &descriptor_table);
    GraphicsPipeline(const GraphicsPipeline &other)             = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other)  = delete;
    HYP_API ~GraphicsPipeline();

    [[nodiscard]]
    HYP_FORCE_INLINE
    GraphicsPipelinePlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const GraphicsPipelinePlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const VertexAttributeSet &GetVertexAttributes() const
        { return vertex_attributes; }

    HYP_FORCE_INLINE
    void SetVertexAttributes(const VertexAttributeSet &vertex_attributes)
        { this->vertex_attributes = vertex_attributes; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Topology GetTopology() const
        { return topology; }

    HYP_FORCE_INLINE
    void SetTopology(Topology topology)
        { this->topology = topology; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    FaceCullMode GetCullMode() const
        { return m_face_cull_mode; }

    HYP_FORCE_INLINE
    void SetCullMode(FaceCullMode face_cull_mode)
        { m_face_cull_mode = face_cull_mode; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    FillMode GetFillMode() const
        { return fill_mode; }

    HYP_FORCE_INLINE
    void SetFillMode(FillMode fill_mode)
        { this->fill_mode = fill_mode; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const BlendFunction &GetBlendFunction() const
        { return blend_function; }

    HYP_FORCE_INLINE
    void SetBlendFunction(const BlendFunction &blend_function)
        { this->blend_function = blend_function; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const StencilFunction &GetStencilFunction() const
        { return stencil_function; }

    HYP_FORCE_INLINE
    void SetStencilFunction(const StencilFunction &stencil_function)
        { this->stencil_function = stencil_function; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool GetDepthTest() const
        { return depth_test; }

    HYP_FORCE_INLINE
    void SetDepthTest(bool depth_test)
        { this->depth_test = depth_test; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool GetDepthWrite() const
        { return depth_write; }

    HYP_FORCE_INLINE
    void SetDepthWrite(bool depth_write)
        { this->depth_write = depth_write; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const RenderPassRef<PLATFORM> &GetRenderPass() const
        { return render_pass; }

    HYP_FORCE_INLINE
    void SetRenderPass(const RenderPassRef<PLATFORM> &render_pass)
        { this->render_pass = render_pass; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Array<FramebufferRef<PLATFORM>> &GetFramebuffers() const
        { return fbos; }

    HYP_FORCE_INLINE
    void SetFramebuffers(const Array<FramebufferRef<PLATFORM>> &fbos)
        { this->fbos = fbos; }

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);
    
    HYP_API void Bind(CommandBuffer<PLATFORM> *cmd);

protected:
    Result Rebuild(Device<PLATFORM> *device);
    
    GraphicsPipelinePlatformImpl<PLATFORM>  m_platform_impl;

    VertexAttributeSet                      vertex_attributes;

    Topology                                topology = Topology::TRIANGLES;
    FaceCullMode                            m_face_cull_mode = FaceCullMode::BACK;
    FillMode                                fill_mode = FillMode::FILL;
    BlendFunction                           blend_function = BlendFunction::None();

    StencilFunction                         stencil_function;

    bool                                    depth_test = true;
    bool                                    depth_write = true;

    RenderPassRef<PLATFORM>                 render_pass;
    Array<FramebufferRef<PLATFORM>>         fbos;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using GraphicsPipeline = platform::GraphicsPipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif
