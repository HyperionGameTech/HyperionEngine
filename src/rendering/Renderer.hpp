#ifndef HYPERION_V2_RENDERER_H
#define HYPERION_V2_RENDERER_H

#include <core/Containers.hpp>
#include <animation/Skeleton.hpp>
#include <scene/Entity.hpp>
#include "Shader.hpp"
#include "Framebuffer.hpp"
#include "RenderPass.hpp"
#include "RenderBucket.hpp"
#include "RenderableAttributes.hpp"
#include "IndirectDraw.hpp"
#include "CullData.hpp"
#include "Compute.hpp"
#include <Constants.hpp>
#include <core/lib/AtomicSemaphore.hpp>

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

class RendererInstance
    : public EngineComponentBase<STUB_CLASS(RendererInstance)>
{
    friend class Engine;
    friend class Entity;

public:
    RendererInstance(
        Handle<Shader> &&shader,
        Handle<RenderPass> &&render_pass,
        const RenderableAttributeSet &renderable_attributes
    );

    RendererInstance(const RendererInstance &other) = delete;
    RendererInstance &operator=(const RendererInstance &other) = delete;
    ~RendererInstance();

    renderer::GraphicsPipeline *GetPipeline() const { return m_pipeline.get(); }

    Handle<Shader> &GetShader() { return m_shader; }
    const Handle<Shader> &GetShader() const { return m_shader; }
    
    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }
    
    void AddEntity(Ref<Entity> &&entity);
    void RemoveEntity(Ref<Entity> &&entity, bool call_on_removed = true);
    auto &GetEntities()                                     { return m_entities; }
    const auto &GetEntities() const                         { return m_entities; }

    void AddFramebuffer(Ref<Framebuffer> &&fbo)             { m_fbos.push_back(std::move(fbo)); }
    void RemoveFramebuffer(Framebuffer::ID id);
    auto &GetFramebuffers()                                 { return m_fbos; } 
    const auto &GetFramebuffers() const                     { return m_fbos; }
    
    void Init(Engine *engine);
    
    void CollectDrawCalls(
        Engine *engine,
        Frame *frame,
        const CullData &cull_data
    );

    void PerformRendering(
        Engine *engine,
        Frame *frame
    );

    // render non-indirect
    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void PerformEnqueuedEntityUpdates(Engine *engine, UInt frame_index);
    
    void UpdateEnqueuedEntitiesFlag()
    {
        m_enqueued_entities_flag.store(
           !m_entities_pending_addition.empty() || !m_entities_pending_removal.empty()
        );
    }

    std::unique_ptr<renderer::GraphicsPipeline> m_pipeline;

    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    RenderableAttributeSet m_renderable_attributes;

    IndirectRenderer m_indirect_renderer;
    
    std::vector<Ref<Framebuffer>> m_fbos;

    std::vector<Ref<Entity>> m_entities; // lives in RENDER thread
    std::vector<Ref<Entity>> m_entities_pending_addition; // shared
    std::vector<Ref<Entity>> m_entities_pending_removal; // shared

    PerFrameData<CommandBuffer> *m_per_frame_data;

    // std::mutex m_enqueued_entities_mutex;
    BinarySemaphore m_enqueued_entities_sp;
    std::atomic_bool m_enqueued_entities_flag { false };

};

} // namespace hyperion::v2

#endif
