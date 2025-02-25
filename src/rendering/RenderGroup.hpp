/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_HPP
#define HYPERION_RENDERER_HPP

#include <core/utilities/EnumFlags.hpp>
#include <core/ID.hpp>
#include <core/Defines.hpp>

#include <rendering/Shader.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/CullData.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>

namespace hyperion {

using renderer::Topology;
using renderer::FillMode;
using renderer::FaceCullMode;

class Engine;
class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderCollector;
class GPUBufferHolderBase;
class IndirectRenderer;

enum class RenderGroupFlags : uint32
{
    NONE                = 0x0,
    OCCLUSION_CULLING   = 0x1,
    INDIRECT_RENDERING  = 0x2,
    PARALLEL_RENDERING  = 0x4,

    DEFAULT             = OCCLUSION_CULLING | INDIRECT_RENDERING | PARALLEL_RENDERING
};

HYP_MAKE_ENUM_FLAGS(RenderGroupFlags)

HYP_CLASS()
class HYP_API RenderGroup : public HypObject<RenderGroup>
{
    HYP_OBJECT_BODY(RenderGroup);

    friend class DebugDrawerRenderGroupProxy;

public:
    using AsyncCommandBuffers = FixedArray<FixedArray<CommandBufferRef, num_async_rendering_command_buffers>, max_frames_in_flight>;

    RenderGroup();

    RenderGroup(
        const ShaderRef &shader,
        const RenderableAttributeSet &renderable_attributes,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT
    );

    RenderGroup(
        const ShaderRef &shader,
        const RenderableAttributeSet &renderable_attributes,
        const DescriptorTableRef &descriptor_table,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT
    );

    RenderGroup(const RenderGroup &other)               = delete;
    RenderGroup &operator=(const RenderGroup &other)    = delete;
    ~RenderGroup();

    HYP_FORCE_INLINE const GraphicsPipelineRef &GetPipeline() const
        { return m_pipeline; }

    HYP_FORCE_INLINE const ShaderRef &GetShader() const
        { return m_shader; }

    void SetShader(const ShaderRef &shader);

    HYP_FORCE_INLINE const RenderableAttributeSet &GetRenderableAttributes() const
        { return m_renderable_attributes; }

    void SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes);

    void AddFramebuffer(const FramebufferRef &framebuffer);
    void RemoveFramebuffer(const FramebufferRef &framebuffer);

    HYP_FORCE_INLINE const Array<FramebufferRef> &GetFramebuffers() const
        { return m_fbos; }

    HYP_FORCE_INLINE const DrawCallCollection &GetDrawState() const
        { return m_draw_state; }

    HYP_FORCE_INLINE EnumFlags<RenderGroupFlags> GetFlags() const
        { return m_flags; }

    void ClearProxies();

    void AddRenderProxy(const RenderProxy &render_proxy);

    bool RemoveRenderProxy(ID<Entity> entity);
    typename FlatMap<ID<Entity>, const RenderProxy *>::Iterator RemoveRenderProxy(typename FlatMap<ID<Entity>, const RenderProxy *>::ConstIterator iterator);

    HYP_FORCE_INLINE const FlatMap<ID<Entity>, const RenderProxy *> &GetRenderProxies() const
        { return m_render_proxies; }

    void SetDrawCallCollectionImpl(IDrawCallCollectionImpl *draw_call_collection_impl);

    /*! \brief Collect drawable objects, then run the culling compute shader
     *  to mark any occluded objects as such. Must be used with indirect rendering.
     *  If nullptr is provided for cull_data, no occlusion culling will happen.
     */
    void CollectDrawCalls();

    /*! \brief Render objects using direct rendering, no occlusion culling is provided. */
    void PerformRendering(Frame *frame);

    /*! \brief Render objects using indirect rendering. The objects must have had the culling shader ran on them,
     * using CollectDrawCalls(). */
    void PerformRenderingIndirect(Frame *frame);

    void PerformOcclusionCulling(Frame *frame, const CullData *cull_data);

    void Init();

private:
    void CreateIndirectRenderer();
    void CreateCommandBuffers();
    void CreateGraphicsPipeline();

    void BindDescriptorSets(
        CommandBuffer *command_buffer,
        uint32 scene_index
    );

    EnumFlags<RenderGroupFlags>                         m_flags;


    GraphicsPipelineRef                                 m_pipeline;

    ShaderRef                                           m_shader;

    DescriptorTableRef                                  m_descriptor_table;

    RenderableAttributeSet                              m_renderable_attributes;

    RC<IndirectRenderer>                                m_indirect_renderer;

    Array<FramebufferRef>                               m_fbos;

    // for each frame in flight - have an array of command buffers to use
    // for async command buffer recording.
    UniquePtr<AsyncCommandBuffers>                      m_command_buffers;

    // cache so we don't allocate every frame
    Array<Span<const DrawCall>>                         m_divided_draw_calls;

    // cycle through command buffers, so you can call Render()
    // multiple times in a single pass, only running into issues if you
    // try to call it more than num_async_rendering_command_buffers
    // (or if parallel rendering is enabled, more than the number of task threads available (usually 2))
    uint32                                              m_command_buffer_index = 0u;

    DrawCallCollection                                  m_draw_state;

    FlatMap<ID<Entity>, const RenderProxy *>            m_render_proxies;
};

} // namespace hyperion

#endif
