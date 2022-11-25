#include "Renderer.hpp"
#include <Engine.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateGraphicsPipeline) : RenderCommandBase2
{
    renderer::GraphicsPipeline *pipeline;
    renderer::ShaderProgram *shader_program;
    renderer::RenderPass *render_pass;
    Array<renderer::FramebufferObject *> framebuffers;
    Array<Array<renderer::CommandBuffer *>> command_buffers;
    RenderableAttributeSet attributes;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        renderer::GraphicsPipeline *pipeline,
        renderer::ShaderProgram *shader_program,
        renderer::RenderPass *render_pass,
        Array<renderer::FramebufferObject *> &&framebuffers,
        Array<Array<renderer::CommandBuffer *>> &&command_buffers,
        const RenderableAttributeSet &attributes
    ) : pipeline(pipeline),
        shader_program(shader_program),
        render_pass(render_pass),
        framebuffers(std::move(framebuffers)),
        command_buffers(std::move(command_buffers)),
        attributes(attributes)
    {
    }

    virtual Result operator()()
    {
        renderer::GraphicsPipeline::ConstructionInfo construction_info {
            .vertex_attributes = attributes.mesh_attributes.vertex_attributes,
            .topology          = attributes.mesh_attributes.topology,
            .cull_mode         = attributes.material_attributes.cull_faces,
            .fill_mode         = attributes.material_attributes.fill_mode,
            .depth_test        = bool(attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST),
            .depth_write       = bool(attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE),
            .blend_enabled     = bool(attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING),
            .shader            = shader_program,
            .render_pass       = render_pass,
            .stencil_state     = attributes.stencil_state
        };

        for (renderer::FramebufferObject *framebuffer : framebuffers) {
            construction_info.fbos.push_back(framebuffer);
        }

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            for (UInt j = 0; j < UInt(command_buffers[i].Size()); j++) {
                HYPERION_BUBBLE_ERRORS(command_buffers[i][j]->Create(
                    Engine::Get()->GetInstance()->GetDevice(),
                    Engine::Get()->GetInstance()->GetGraphicsCommandPool(j)
                ));
            }
        }

        return pipeline->Create(
            Engine::Get()->GetDevice(),
            std::move(construction_info),
            &Engine::Get()->GetInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(DestroyGraphicsPipeline) : RenderCommandBase2
{
    renderer::GraphicsPipeline *pipeline;

    RENDER_COMMAND(DestroyGraphicsPipeline)(renderer::GraphicsPipeline *pipeline)
        : pipeline(pipeline)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Destroy(Engine::Get()->GetDevice());
    }
};

RendererInstance::RendererInstance(
    Handle<Shader> &&shader,
    Handle<RenderPass> &&render_pass,
    const RenderableAttributeSet &renderable_attributes
) : EngineComponentBase(),
    m_pipeline(std::make_unique<renderer::GraphicsPipeline>()),
    m_shader(std::move(shader)),
    m_render_pass(std::move(render_pass)),
    m_renderable_attributes(renderable_attributes)
{
}

RendererInstance::RendererInstance(
    Handle<Shader> &&shader,
    Handle<RenderPass> &&render_pass,
    const RenderableAttributeSet &renderable_attributes,
    const Array<const DescriptorSet *> &used_descriptor_sets
) : EngineComponentBase(),
    m_pipeline(std::make_unique<renderer::GraphicsPipeline>(used_descriptor_sets)),
    m_shader(std::move(shader)),
    m_render_pass(std::move(render_pass)),
    m_renderable_attributes(renderable_attributes)
{
}


RendererInstance::~RendererInstance()
{
    Teardown();
}

void RendererInstance::RemoveFramebuffer(Framebuffer::ID id)
{
    const auto it = m_fbos.FindIf([&](const auto &item) {
        return item->GetID() == id;
    });

    if (it == m_fbos.End()) {
        return;
    }
    
    m_fbos.Erase(it);
}

void RendererInstance::AddEntity(Handle<Entity> &&entity)
{
    AssertThrow(entity != nullptr);
    // AssertThrow(entity->IsReady());

    // AssertThrow(entity->IsRenderable());

    // FIXME: thread safety. this could be called from any thread
    AssertThrowMsg(
        (m_renderable_attributes.mesh_attributes.vertex_attributes
            & entity->GetRenderableAttributes().mesh_attributes.vertex_attributes)
                == entity->GetRenderableAttributes().mesh_attributes.vertex_attributes,
        "RendererInstance vertex attributes does not satisfy the required vertex attributes of the entity."
    );

    if (IsInitCalled()) {
        Engine::Get()->InitObject(entity);
    }

    entity->OnAddedToPipeline(this);
    
    m_enqueued_entities_sp.Wait();

    // remove from 'pending removal' list
    auto it = m_entities_pending_removal.Find(entity);

    if (it != m_entities_pending_removal.End()) {
        m_entities_pending_removal.Erase(it);
    }
    
    m_entities_pending_addition.PushBack(std::move(entity));

    m_enqueued_entities_sp.Signal();
    
    UpdateEnqueuedEntitiesFlag();
}

void RendererInstance::RemoveEntity(Handle<Entity> &&entity, bool call_on_removed)
{
    // we cannot touch m_entities from any thread other than the render thread
    // we'll have to assume it's here, and check later :/ 

    // add it to pending removal list
    // std::lock_guard guard(m_enqueued_entities_mutex);

    m_enqueued_entities_sp.Wait();

    const auto pending_removal_it = m_entities_pending_removal.Find(entity);

    if (pending_removal_it == m_entities_pending_removal.End()) {
        if (call_on_removed) {
            entity->OnRemovedFromPipeline(this);
        }

        auto pending_addition_it = m_entities_pending_addition.Find(entity);

        if (pending_addition_it != m_entities_pending_addition.End()) {
            
            // directly remove from list of ones pending addition
            m_entities_pending_addition.Erase(pending_addition_it);
        } else {
            m_entities_pending_removal.PushBack(std::move(entity));
        }

        UpdateEnqueuedEntitiesFlag();
    } else {
        DebugLog(
            LogType::Info,
            "Entity #%u is already pending removal from pipeline #%u\n",
            entity->GetID().value,
            m_id.value
        );
    }

    m_enqueued_entities_sp.Signal();
}

void RendererInstance::PerformEnqueuedEntityUpdates( UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    // std::lock_guard guard(m_enqueued_entities_mutex);

    m_enqueued_entities_sp.Wait();

    if (m_entities_pending_removal.Any()) {
        for (auto it = m_entities_pending_removal.Begin(); it != m_entities_pending_removal.End();) {
            const auto entities_it = m_entities.Find(*it); // oof
            
            if (entities_it != m_entities.End()) {
                m_entities.Erase(entities_it);
            }

            it = m_entities_pending_removal.Erase(it);
        }
    }

    if (m_entities_pending_addition.Any()) {
        // we only add entities that are fully ready,
        // keeping ones that aren't finished initializing in the vector,
        // they should be finished on the next pass
        
        auto it = m_entities_pending_addition.Begin();
        
        while (it != m_entities_pending_addition.End()) {
            AssertThrow(*it != nullptr);
            // TODO: Swap with drawproxy Mesh?
            if ((*it)->GetMesh() == nullptr // TODO: I don't believe it's threadsafe to just check if this is null like this
                || m_entities.Find(*it) != m_entities.End()) { // :(
                it = m_entities_pending_addition.Erase(it);
                
                continue;
            }
            
            if ((*it)->IsReady() && (*it)->GetMesh()->IsReady()) {
                Engine::Get()->InitObject(*it);
                m_entities.PushBack(std::move(*it));
                it = m_entities_pending_addition.Erase(it);
                
                continue;
            }
            
            // not ready, keep looping.
            ++it;
        }
    }

    m_enqueued_entities_sp.Signal();

    UpdateEnqueuedEntitiesFlag();
}

void RendererInstance::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init;

    // create our indirect renderer
    // will be created with some initial size.
    m_indirect_renderer.Create;

    AssertThrow(m_fbos.Any());

    for (auto &fbo : m_fbos) {
        AssertThrow(fbo != nullptr);
        Engine::Get()->InitObject(fbo);
    }

    AssertThrow(m_shader != nullptr);
    Engine::Get()->InitObject(m_shader);

    for (auto &&entity : m_entities) {
        AssertThrow(entity != nullptr);
        Engine::Get()->InitObject(entity);
    }

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (auto &command_buffer : m_command_buffers[i]) {
            command_buffer.Reset(new CommandBuffer(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY));
        }
    }

    OnInit(Engine::Get()->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        Array<renderer::FramebufferObject *> framebuffers;
        framebuffers.Reserve(m_fbos.Size());

        for (auto &fbo : m_fbos) {
            framebuffers.PushBack(&fbo->GetFramebuffer());
        }

        Array<Array<renderer::CommandBuffer *>> command_buffers;
        command_buffers.Reserve(m_command_buffers.Size());
        
        for (auto &item : m_command_buffers) {
            Array<renderer::CommandBuffer *> frame_command_buffers;
            frame_command_buffers.Reserve(item.Size());

            for (auto &command_buffer : item) {
                frame_command_buffers.PushBack(command_buffer.Get());
            }

            command_buffers.PushBack(std::move(frame_command_buffers));
        }

        RenderCommands::Push<RENDER_COMMAND(CreateGraphicsPipeline)>(
            m_pipeline.get(),
            m_shader->GetShaderProgram(),
            &m_render_pass->GetRenderPass(),
            std::move(framebuffers),
            std::move(command_buffers),
            m_renderable_attributes
        );
            
        SetReady(true);

        OnTeardown([this]() {
            SetReady(false);

            m_indirect_renderer.Destroy; // make sure we have the render queue flush at the end of
                                                 // this, as the indirect renderer has a call back that needs to be exec'd
                                                 // before the destructor is called

            for (auto &&entity : m_entities) {
                AssertThrow(entity != nullptr);

                entity->OnRemovedFromPipeline(this);
            }

            m_entities.Clear();

            {
                m_enqueued_entities_sp.Wait();

                for (auto &&entity : m_entities_pending_addition) {
                    if (entity == nullptr) {
                        continue;
                    }

                    entity->OnRemovedFromPipeline(this);
                }

                m_entities_pending_addition.Clear();
                m_entities_pending_removal.Clear();

                m_enqueued_entities_sp.Signal();
            }

            m_shader.Reset();

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                for (UInt i = 0; i < UInt(m_command_buffers[frame_index].Size()); i++) {
                    Engine::Get()->SafeRelease(std::move(m_command_buffers[frame_index][i]));
                }
            }

            for (auto &fbo : m_fbos) {
                fbo.Reset();
            }

            RenderCommands::Push<RENDER_COMMAND(DestroyGraphicsPipeline)>(m_pipeline.get());
            
            HYP_FLUSH_RENDER_QUEUE();
        });
    }));
}

void RendererInstance::CollectDrawCalls( Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (m_enqueued_entities_flag.load()) {
        PerformEnqueuedEntityUpdatesframe->GetFrameIndex());
    }
    
    m_indirect_renderer.GetDrawState().Reset();
    m_divided_draw_proxies.Clear();

    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const auto scene_id = scene_binding.id;
    const auto scene_index = scene_binding ? scene_binding.id.value - 1 : 0;

    // check visibility state
    const bool perform_culling = scene_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.material_attributes.bucket);
    const auto visibility_cursor = Engine::Get()->render_state.visibility_cursor;
    const auto &octree_visibility_state_snapshot = Engine::Get()->GetWorld()->GetOctree().GetVisibilityState().snapshots[visibility_cursor];

    for (auto &&entity : m_entities) {
        if (entity->GetMesh() == nullptr) {
            continue;
        }

        if (perform_culling) {
            const auto &snapshot = entity->GetVisibilityState().snapshots[visibility_cursor];

            if (!snapshot.ValidToParent(octree_visibility_state_snapshot)) {
                continue;
            }

            if (!snapshot.Get(scene_id)) {
                continue;
            }
        }
        
        m_indirect_renderer.GetDrawState().PushDrawProxy(entity->GetDrawProxy());
    }
}


void RendererInstance::CollectDrawCalls(
    
    Frame *frame,
    const CullData &cull_data
)
{
    CollectDrawCallsframe);

    m_indirect_renderer.ExecuteCullShaderInBatches(
        Engine::Get(),
        frame,
        cull_data
    );
}

static void GetDividedDrawCalls(
    const Array<EntityDrawProxy> &draw_proxies,
    UInt num_batches,
    Array<Array<EntityDrawProxy>> &out_divided_draw_proxies
)
{
    out_divided_draw_proxies.Resize(num_batches);

    const UInt num_draw_proxies = static_cast<UInt>(draw_proxies.Size());
    const UInt num_draw_proxies_divided = (num_draw_proxies + num_batches - 1) / num_batches;

    UInt draw_proxy_index = 0;

    for (SizeType container_index = 0; container_index < num_async_rendering_command_buffers; container_index++) {
        auto &container = out_divided_draw_proxies[container_index];
        container.Reserve(num_draw_proxies_divided);

        for (SizeType i = 0; i < num_draw_proxies_divided && draw_proxy_index < num_draw_proxies; i++, draw_proxy_index++) {
            container.PushBack(draw_proxies[draw_proxy_index]);
        }
    }
}

static void BindGlobalDescriptorSets(
    
    Frame *frame,
    renderer::GraphicsPipeline *pipeline,
    CommandBuffer *command_buffer,
    UInt scene_index
)
{
    const UInt frame_index = frame->GetFrameIndex();

    command_buffer->BindDescriptorSets(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        pipeline,
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
        FixedArray {
            UInt32(scene_index * sizeof(SceneShaderData)),
            HYP_RENDER_OBJECT_OFFSET(Light, 0)
        }
    );

#if HYP_FEATURES_BINDLESS_TEXTURES
    /* Bindless textures */
    Engine::Get()->GetInstance()->GetDescriptorPool().Bind(
        Engine::Get()->GetDevice(),
        command_buffer,
        pipeline,
        {
            {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
        }
    );
#endif
                    
    Engine::Get()->GetInstance()->GetDescriptorPool().Bind(
        Engine::Get()->GetDevice(),
        command_buffer,
        pipeline,
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
        }
    );
}

static void BindPerObjectDescriptorSets(
    
    Frame *frame,
    renderer::GraphicsPipeline *pipeline,
    CommandBuffer *command_buffer,
    UInt entity_index,
    UInt skeleton_index,
    UInt material_index
)
{
    const UInt frame_index = frame->GetFrameIndex();
    
#if HYP_FEATURES_BINDLESS_TEXTURES
    command_buffer->BindDescriptorSets(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        pipeline,
        FixedArray<DescriptorSet::Index, 1> { DescriptorSet::object_buffer_mapping[frame_index] },
        FixedArray<DescriptorSet::Index, 1> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
        FixedArray {
            UInt32(material_index * sizeof(MaterialShaderData)),
            UInt32(entity_index * sizeof(ObjectShaderData)),
            UInt32(skeleton_index * sizeof(SkeletonShaderData))
        }
    );
#else
    command_buffer->BindDescriptorSets(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        pipeline,
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT, DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
        FixedArray {
            UInt32(material_index * sizeof(MaterialShaderData)),
            UInt32(entity_index * sizeof(ObjectShaderData)),
            UInt32(skeleton_index * sizeof(SkeletonShaderData))
        }
    );
#endif
}

template <bool IsIndirect>
static HYP_FORCE_INLINE void
RenderAll(
    
    Frame *frame,
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> &command_buffers,
    UInt &command_buffer_index,
    renderer::GraphicsPipeline *pipeline,
    IndirectRenderer *indirect_renderer,
    const Array<EntityDrawProxy> &draw_proxies,
    Array<Array<EntityDrawProxy>> &divided_draw_proxies
)
{
    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const auto scene_id = scene_binding.id;

    auto *instance = Engine::Get()->GetInstance();
    auto *device = instance->GetDevice();

    const UInt frame_index = frame->GetFrameIndex();

    const auto num_batches = RendererInstance::parallel_rendering
        ? MathUtil::Min(static_cast<UInt>(Engine::Get()->task_system.GetPool(TaskPriority::HIGH).threads.Size()), num_async_rendering_command_buffers)
        : 1u;
    
    GetDividedDrawCalls(
        draw_proxies,
        num_async_rendering_command_buffers,
        divided_draw_proxies
    );

    // rather than using a single integer, we have to set states in a fixed array
    // because otherwise we'd need to use an atomic integer
    FixedArray<UInt, num_async_rendering_command_buffers> command_buffers_recorded_states { };
    
    // always run renderer items as HIGH priority,
    // so we do not lock up because we're waiting for a large process to
    // complete in the same thread
    Engine::Get()->task_system.ParallelForEach(
        TaskPriority::HIGH,
        num_batches,
        divided_draw_proxies,
        [frame, pipeline, indirect_renderer, &command_buffers, &command_buffers_recorded_states, command_buffer_index, frame_index, scene_id](const Array<EntityDrawProxy> &draw_proxies, UInt index, UInt) {
            if (draw_proxies.Empty()) {
                return;
            }

            command_buffers[frame_index][index/*(command_buffer_index + batch_index) % static_cast<UInt>(command_buffers.Size())*/]->Record(
                Engine::Get()->GetDevice(),
                pipeline->GetConstructionInfo().render_pass,
                [&](CommandBuffer *secondary) {
                    pipeline->Bind(secondary);

                    BindGlobalDescriptorSets(
                        Engine::Get(),
                        frame,
                        pipeline,
                        secondary,
                        scene_id.ToIndex()
                    );
                    
                    for (const auto &draw_proxy : draw_proxies) {
                        BindPerObjectDescriptorSets(
                            Engine::Get(),
                            frame,
                            pipeline,
                            secondary,
                            draw_proxy.entity_id.ToIndex(),
                            draw_proxy.skeleton_id.ToIndex(),
                            draw_proxy.material_id.ToIndex()
                        );

                        if constexpr (IsIndirect) {
#ifdef HYP_DEBUG_MODE
                            AssertThrow(draw_proxy.draw_command_index * sizeof(IndirectDrawCommand) < indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index)->size);
#endif
                            
                            draw_proxy.mesh->RenderIndirect(
                                Engine::Get(),
                                secondary,
                                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                                draw_proxy.draw_command_index * sizeof(IndirectDrawCommand)
                            );
                        } else {
                            draw_proxy.mesh->Rendersecondary);
                        }
                    }

                    HYPERION_RETURN_OK;
                }
            );

            command_buffers_recorded_states[index] = 1u;
        }
    );

    const auto num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (UInt i = 0; i < num_recorded_command_buffers; i++) {
        command_buffers[frame_index][/*(command_buffer_index + i) % static_cast<UInt>(command_buffers.Size())*/ i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }

    command_buffer_index = (command_buffer_index + num_recorded_command_buffers) % static_cast<UInt>(command_buffers.Size());
}

void RendererInstance::PerformRendering( Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    RenderAll<false>(
        Engine::Get(),
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline.get(),
        &m_indirect_renderer,
        m_indirect_renderer.GetDrawState().GetDrawProxies(),
        m_divided_draw_proxies
    );
}

void RendererInstance::PerformRenderingIndirect( Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    RenderAll<true>(
        Engine::Get(),
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline.get(),
        &m_indirect_renderer,
        m_indirect_renderer.GetDrawState().GetDrawProxies(),
        m_divided_draw_proxies
    );
}

void RendererInstance::Render( Frame *frame)
{
    // perform all ops in one batch
    CollectDrawCallsframe);
    PerformRenderingframe);
}

// Proxied methods


CommandBuffer *RendererProxy::GetCommandBuffer(UInt frame_index)
{
    return m_renderer_instance->m_command_buffers[frame_index].Front().Get();
}

renderer::GraphicsPipeline *RendererProxy::GetGraphicsPipeline()
{
    return m_renderer_instance->m_pipeline.get();
}

void RendererProxy::Bind( Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    CommandBuffer *command_buffer = m_renderer_instance->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    command_buffer->Begin(Engine::Get()->GetDevice(), m_renderer_instance->m_pipeline->GetConstructionInfo().render_pass);

    m_renderer_instance->m_pipeline->Bind(command_buffer);
}

void RendererProxy::SetConstants(void *ptr, SizeType size)
{
#ifdef HYP_DEBUG_MODE
    AssertThrowMsg(size <= 128, "Size of push constants must be <= 128");
#endif

    m_renderer_instance->m_pipeline->SetPushConstants(ptr, size);
}

void RendererProxy::BindEntityObject(
    
    Frame *frame,
    const EntityDrawProxy &entity
)
{
    BindPerObjectDescriptorSets(
        Engine::Get(),
        frame,
        m_renderer_instance->m_pipeline.get(),
        m_renderer_instance->m_command_buffers[frame->GetFrameIndex()].Front().Get(),
        entity.entity_id.ToIndex(),
        entity.skeleton_id.ToIndex(),
        entity.material_id.ToIndex()
    );
}

void RendererProxy::DrawMesh(
    
    Frame *frame,
    Mesh *mesh
)
{
    CommandBuffer *command_buffer = m_renderer_instance->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    mesh->Rendercommand_buffer);
}

void RendererProxy::Submit(
    
    Frame *frame
)
{
    CommandBuffer *command_buffer = m_renderer_instance->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    command_buffer->End(Engine::Get()->GetDevice());
    command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
