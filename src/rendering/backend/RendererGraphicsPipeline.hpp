/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP
#define HYPERION_BACKEND_RENDERER_GRAPHICS_PIPELINE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererStructs.hpp>

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
    static constexpr PlatformType platform = PLATFORM;
    
    HYP_API GraphicsPipeline();
    HYP_API GraphicsPipeline(const ShaderRef<PLATFORM> &shader, const DescriptorTableRef<PLATFORM> &descriptor_table);
    GraphicsPipeline(const GraphicsPipeline &other)             = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other)  = delete;
    HYP_API ~GraphicsPipeline();

    HYP_FORCE_INLINE GraphicsPipelinePlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const GraphicsPipelinePlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE const VertexAttributeSet &GetVertexAttributes() const
        { return m_vertex_attributes; }

    HYP_FORCE_INLINE void SetVertexAttributes(const VertexAttributeSet &vertex_attributes)
        { m_vertex_attributes = vertex_attributes; }

    HYP_FORCE_INLINE Topology GetTopology() const
        { return m_topology; }

    HYP_FORCE_INLINE void SetTopology(Topology topology)
        { m_topology = topology; }

    HYP_FORCE_INLINE FaceCullMode GetCullMode() const
        { return m_face_cull_mode; }

    HYP_FORCE_INLINE void SetCullMode(FaceCullMode face_cull_mode)
        { m_face_cull_mode = face_cull_mode; }

    HYP_FORCE_INLINE FillMode GetFillMode() const
        { return m_fill_mode; }

    HYP_FORCE_INLINE void SetFillMode(FillMode fill_mode)
        { m_fill_mode = fill_mode; }

    HYP_FORCE_INLINE const BlendFunction &GetBlendFunction() const
        { return m_blend_function; }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction &blend_function)
        { m_blend_function = blend_function; }

    HYP_FORCE_INLINE const StencilFunction &GetStencilFunction() const
        { return m_stencil_function; }

    HYP_FORCE_INLINE void SetStencilFunction(const StencilFunction &stencil_function)
        { m_stencil_function = stencil_function; }

    HYP_FORCE_INLINE bool GetDepthTest() const
        { return m_depth_test; }

    HYP_FORCE_INLINE void SetDepthTest(bool depth_test)
        { m_depth_test = depth_test; }

    HYP_FORCE_INLINE bool GetDepthWrite() const
        { return m_depth_write; }

    HYP_FORCE_INLINE void SetDepthWrite(bool depth_write)
        { m_depth_write = depth_write; }

    HYP_FORCE_INLINE const RenderPassRef<PLATFORM> &GetRenderPass() const
        { return m_render_pass; }

    HYP_API void SetRenderPass(const RenderPassRef<PLATFORM> &render_pass);

    HYP_FORCE_INLINE const Array<FramebufferRef<PLATFORM>> &GetFramebuffers() const
        { return m_framebuffers; }

    HYP_API void SetFramebuffers(const Array<FramebufferRef<PLATFORM>> &framebuffers);

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);
    
    HYP_API void Bind(CommandBuffer<PLATFORM> *cmd);

protected:
    Result Rebuild(Device<PLATFORM> *device);
    
    GraphicsPipelinePlatformImpl<PLATFORM>  m_platform_impl;

    VertexAttributeSet                      m_vertex_attributes;

    Topology                                m_topology = Topology::TRIANGLES;
    FaceCullMode                            m_face_cull_mode = FaceCullMode::BACK;
    FillMode                                m_fill_mode = FillMode::FILL;
    BlendFunction                           m_blend_function = BlendFunction::None();

    StencilFunction                         m_stencil_function;

    bool                                    m_depth_test = true;
    bool                                    m_depth_write = true;

    RenderPassRef<PLATFORM>                 m_render_pass;
    Array<FramebufferRef<PLATFORM>>         m_framebuffers;
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
