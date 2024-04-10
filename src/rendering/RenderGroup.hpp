#ifndef HYPERION_V2_RENDERER_H
#define HYPERION_V2_RENDERER_H

#include <core/Containers.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/EntityDrawData.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/CullData.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/Mesh.hpp>
#include <Constants.hpp>
#include <core/ID.hpp>
#include <util/Defines.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <memory>
#include <mutex>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Topology;
using renderer::FillMode;
using renderer::FaceCullMode;
using renderer::StencilState;
using renderer::Frame;
using renderer::Pipeline;

class Engine;
class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderList;

/*! \brief Represents a handle to a graphics pipeline,
    which can be used for doing standalone drawing without requiring
    all objects to be Entities or have them attached to the RenderGroup */
class HYP_API RendererProxy
{
    friend class RenderGroup;

    RendererProxy(RenderGroup *renderer_instance)
        : m_render_group(renderer_instance)
    {
    }

    RendererProxy(const RendererProxy &other) = delete;
    RendererProxy &operator=(const RendererProxy &other) = delete;

public:
    const CommandBufferRef &GetCommandBuffer(uint frame_index) const;

    const GraphicsPipelineRef &GetGraphicsPipeline() const;

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void Bind(Frame *frame);

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void DrawMesh(Frame *frame, Mesh *mesh);

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void Submit(Frame *frame);

private:
    RenderGroup *m_render_group;
};

class HYP_API RenderGroup
    : public BasicObject<STUB_CLASS(RenderGroup)>
{
    friend class Engine;
    friend class Entity;
    friend class RendererProxy;
    friend class RenderList;
public:
    using AsyncCommandBuffers = FixedArray<FixedArray<CommandBufferRef, num_async_rendering_command_buffers>, max_frames_in_flight>;

    RenderGroup(
        Handle<Shader> shader,
        const RenderableAttributeSet &renderable_attributes
    );

    RenderGroup(
        Handle<Shader> shader,
        const RenderableAttributeSet &renderable_attributes,
        DescriptorTableRef descriptor_table
    );

    RenderGroup(const RenderGroup &other) = delete;
    RenderGroup &operator=(const RenderGroup &other) = delete;
    ~RenderGroup();

    const GraphicsPipelineRef &GetPipeline() const
        { return m_pipeline; }

    const Handle<Shader> &GetShader() const
        { return m_shader; }

    const RenderableAttributeSet &GetRenderableAttributes() const
        { return m_renderable_attributes; }

    void AddFramebuffer(Handle<Framebuffer> &&fbo) { m_fbos.PushBack(std::move(fbo)); }
    void RemoveFramebuffer(ID<Framebuffer> id);
    Array<Handle<Framebuffer>> &GetFramebuffers() { return m_fbos; }
    const Array<Handle<Framebuffer>> &GetFramebuffers() const { return m_fbos; }

    void Init();

    void SetEntityDrawDatas(Array<EntityDrawData> entity_draw_datas);

    // render non-indirect (collects draw calls, then renders)
    void Render(Frame *frame);

    RendererProxy GetProxy()
        { return RendererProxy(this); }

private:
    /*! \brief Collect drawable objects, then run the culling compute shader
     * to mark any occluded objects as such. Must be used with indirect rendering.
     * If nullptr is provided for cull_data, no occlusion culling will happen.
     */
    void CollectDrawCalls();

    void PerformOcclusionCulling(Frame *frame, const CullData *cull_data);

    /*! \brief Render objects using direct rendering, no occlusion culling is provided. */
    void PerformRendering(Frame *frame);

    /*! \brief Render objects using indirect rendering. The objects must have had the culling shader ran on them,
     * using CollectDrawCalls(). */
    void PerformRenderingIndirect(Frame *frame);

    void BindDescriptorSets(
        CommandBuffer *command_buffer,
        uint scene_index
    );

    GraphicsPipelineRef m_pipeline;

    Handle<Shader> m_shader;
    RenderableAttributeSet m_renderable_attributes;

    RC<IndirectRenderer> m_indirect_renderer;

    Array<Handle<Framebuffer>> m_fbos;

    // for each frame in flight - have an array of command buffers to use
    // for async command buffer recording.
    AsyncCommandBuffers m_command_buffers;

    // cache so we don't allocate every frame
    Array<Span<const DrawCall>> m_divided_draw_calls;

    // cycle through command buffers, so you can call Render()
    // multiple times in a single pass, only running into issues if you
    // try to call it more than num_async_rendering_command_buffers
    // (or if parallel rendering is enabled, more than the number of task threads available (usually 2))
    uint m_command_buffer_index = 0u;

    FlatMap<uint, BufferTicket<EntityInstanceBatch>> m_entity_batches;

    Array<EntityDrawData> m_entity_draw_datas;

    DrawCallCollection m_draw_state;
};

} // namespace hyperion::v2

#endif
