#ifndef HYPERION_V2_RENDERER_H
#define HYPERION_V2_RENDERER_H

#include <core/Containers.hpp>
#include <scene/Entity.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/CullData.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/Mesh.hpp>
#include <Constants.hpp>
#include <core/ID.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <util/Defines.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <memory>
#include <mutex>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;
using renderer::DescriptorSetBinding;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::VertexAttributeSet;
using renderer::PerFrameData;
using renderer::Topology;
using renderer::FillMode;
using renderer::FaceCullMode;
using renderer::StencilState;
using renderer::Frame;
using renderer::Pipeline;
using renderer::Extent3D;
using renderer::StorageBuffer;

class Engine;
class Mesh;
class Material;
class Skeleton;

/*! \brief Represents a handle to a graphics pipeline,
    which can be used for doing standalone drawing without requiring
    all objects to be Entities or have them attached to the RenderGroup */
class RendererProxy
{
    friend class RenderGroup;

    RendererProxy(RenderGroup *renderer_instance)
        : m_render_group(renderer_instance)
    {
    }

    RendererProxy(const RendererProxy &other) = delete;
    RendererProxy &operator=(const RendererProxy &other) = delete;

public:
    CommandBuffer *GetCommandBuffer(UInt frame_index);
    renderer::GraphicsPipeline *GetGraphicsPipeline();

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void Bind(Frame *frame);
    
    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void SetConstants(void *ptr, SizeType size);

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void DrawMesh(Frame *frame, Mesh *mesh);

    /*! \brief For using this RenderGroup as a standalone graphics pipeline that will simply
        be bound, with all draw calls recorded elsewhere. */
    void Submit(Frame *frame);

private:
    RenderGroup *m_render_group;
};

class RenderGroup
    : public EngineComponentBase<STUB_CLASS(RenderGroup)>
{
    friend class Engine;
    friend class Entity;
    friend class RendererProxy;

public:
    static constexpr bool parallel_rendering = HYP_FEATURES_PARALLEL_RENDERING;

    RenderGroup(
        Handle<Shader> &&shader,
        const RenderableAttributeSet &renderable_attributes
    );

    RenderGroup(
        Handle<Shader> &&shader,
        const RenderableAttributeSet &renderable_attributes,
        const Array<const DescriptorSet *> &used_descriptor_sets
    );

    RenderGroup(const RenderGroup &other) = delete;
    RenderGroup &operator=(const RenderGroup &other) = delete;
    ~RenderGroup();

    renderer::GraphicsPipeline *GetPipeline() const { return m_pipeline.get(); }

    Handle<Shader> &GetShader() { return m_shader; }
    const Handle<Shader> &GetShader() const { return m_shader; }
    
    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }
    
    void AddEntity(Handle<Entity> &&entity);
    void RemoveEntity(Handle<Entity> &&entity, bool call_on_removed = true);
    auto &GetEntities() { return m_entities; }
    const auto &GetEntities() const { return m_entities; }

    void AddFramebuffer(Handle<Framebuffer> &&fbo) { m_fbos.PushBack(std::move(fbo)); }
    void RemoveFramebuffer(ID<Framebuffer> id);
    Array<Handle<Framebuffer>> &GetFramebuffers() { return m_fbos; } 
    const Array<Handle<Framebuffer>> &GetFramebuffers() const { return m_fbos; }
    
    void Init();

    /*! \brief Collect drawable objects, must only be used with non-indirect rendering!
     */
    void CollectDrawCalls(Frame *frame);

    /*! \brief Collect drawable objects, then run the culling compute shader
     * to mark any occluded objects as such. Must be used with indirect rendering.
     */
    void CollectDrawCalls(
        Frame *frame,
        const CullData &cull_data
    );

    /*! \brief Render objects using direct rendering, no occlusion culling is provided. */
    void PerformRendering(Frame *frame);
    
    /*! \brief Render objects using indirect rendering. The objects must have had the culling shader ran on them,
     * using CollectDrawCalls(). */
    void PerformRenderingIndirect(Frame *frame);

    // render non-indirect (collects draw calls, then renders)
    void Render(Frame *frame);

    RendererProxy GetProxy()
        { return RendererProxy(this); }

private:
    void BindDescriptorSets(
        
        CommandBuffer *command_buffer,
        UInt scene_index
    );

    void PerformEnqueuedEntityUpdates(UInt frame_index);
    
    void UpdateEnqueuedEntitiesFlag()
    {
        m_enqueued_entities_flag.store(
           m_entities_pending_addition.Any() || m_entities_pending_removal.Any()
        );
    }

    std::unique_ptr<renderer::GraphicsPipeline> m_pipeline;

    Handle<Shader> m_shader;
    RenderableAttributeSet m_renderable_attributes;

    IndirectRenderer m_indirect_renderer;
    
    Array<Handle<Framebuffer>> m_fbos;

    Array<Handle<Entity>> m_entities; // lives in RENDER thread
    Array<Handle<Entity>> m_entities_pending_addition; // shared
    Array<Handle<Entity>> m_entities_pending_removal; // shared

    // for each frame in flight - have an array of command buffers to use
    // for async command buffer recording.
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> m_command_buffers;

    // std::mutex m_enqueued_entities_mutex;
    BinarySemaphore m_enqueued_entities_sp;
    std::atomic_bool m_enqueued_entities_flag { false };

    // cache so we don't allocate every frame
    Array<Array<DrawCall>> m_divided_draw_calls;

    // cycle through command buffers, so you can call Render()
    // multiple times in a single pass, only running into issues if you
    // try to call it more than num_async_rendering_command_buffers
    // (or if parallel rendering is enabled, more than the number of task threads available (usually 2))
    UInt m_command_buffer_index = 0u;

    FlatMap<UInt, EntityBatchIndex> m_entity_batches;

    DrawState m_draw_state;
};

} // namespace hyperion::v2

#endif
