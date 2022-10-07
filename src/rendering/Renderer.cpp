#include "Renderer.hpp"
#include <Engine.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

namespace hyperion::v2 {

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
        GetEngine()->InitObject(entity);
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

void RendererInstance::PerformEnqueuedEntityUpdates(Engine *engine, UInt frame_index)
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

            if ((*it)->GetMesh() == nullptr // TODO: I don't believe it's threadsafe to just check if this is null like this
                || m_entities.Find(*it) != m_entities.End()) { // :(
                it = m_entities_pending_addition.Erase(it);
                
                continue;
            }
            
            if ((*it)->IsReady() && (*it)->GetMesh()->IsReady()) {
                engine->InitObject(*it);
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

void RendererInstance::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    // create our indirect renderer
    // will be created with some initial size.
    m_indirect_renderer.Create(engine);

    AssertThrow(m_fbos.Any());

    for (auto &fbo : m_fbos) {
        AssertThrow(fbo != nullptr);
        engine->InitObject(fbo);
    }

    AssertThrow(m_shader != nullptr);
    engine->InitObject(m_shader);

    for (auto &&entity : m_entities) {
        AssertThrow(entity != nullptr);
        engine->InitObject(entity);
    }

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (auto &command_buffer : m_command_buffers[i]) {
            command_buffer.Reset(new CommandBuffer(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY));
        }
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        auto *engine = GetEngine();

        engine->render_scheduler.Enqueue([this, engine](...) {
            renderer::GraphicsPipeline::ConstructionInfo construction_info {
                .vertex_attributes = m_renderable_attributes.mesh_attributes.vertex_attributes,
                .topology          = m_renderable_attributes.mesh_attributes.topology,
                .cull_mode         = m_renderable_attributes.material_attributes.cull_faces,
                .fill_mode         = m_renderable_attributes.material_attributes.fill_mode,
                .depth_test        = bool(m_renderable_attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST),
                .depth_write       = bool(m_renderable_attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE),
                .blend_enabled     = bool(m_renderable_attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING),
                .shader            = m_shader->GetShaderProgram(),
                .render_pass       = &m_render_pass->GetRenderPass(),
                .stencil_state     = m_renderable_attributes.stencil_state
            };

            for (auto &fbo : m_fbos) {
                construction_info.fbos.push_back(&fbo->GetFramebuffer());
            }

            for (UInt i = 0; i < max_frames_in_flight; i++) {
                for (UInt j = 0; j < static_cast<UInt>(m_command_buffers[i].Size()); j++) {
                    HYPERION_ASSERT_RESULT(m_command_buffers[i][j]->Create(
                        engine->GetInstance()->GetDevice(),
                        engine->GetInstance()->GetGraphicsCommandPool(j)
                    ));
                }
            }

            HYPERION_BUBBLE_ERRORS(m_pipeline->Create(
                engine->GetDevice(),
                std::move(construction_info),
                &engine->GetInstance()->GetDescriptorPool()
            ));
            
            SetReady(true);
            
            HYPERION_RETURN_OK;
        });

        OnTeardown([this]() {
            auto *engine = GetEngine();

            SetReady(false);

            m_indirect_renderer.Destroy(engine); // make sure we have the render queue flush at the end of
                                                 // this, as the indirect renderer has a call back that needs to be exec'd
                                                 // before the destructor is called

            for (auto &&entity : m_entities) {
                AssertThrow(entity != nullptr);

                entity->OnRemovedFromPipeline(this);
            }

            m_entities.Clear();
            
            // std::lock_guard guard(m_enqueued_entities_mutex);

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

            m_shader.Reset();

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                for (UInt i = 0; i < static_cast<UInt>(m_command_buffers[frame_index].Size()); i++) {
                    engine->SafeRelease(std::move(m_command_buffers[frame_index][i]));
                }
            }

            engine->render_scheduler.Enqueue([this, engine](...) {
                auto result = renderer::Result::OK;

                HYPERION_PASS_ERRORS(m_pipeline->Destroy(engine->GetDevice()), result);

                return result;
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        });
    }));
}

void RendererInstance::CollectDrawCalls(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (m_enqueued_entities_flag.load()) {
        PerformEnqueuedEntityUpdates(engine, frame->GetFrameIndex());
    }
    
    m_indirect_renderer.GetDrawState().Reset();
    m_divided_draw_proxies.Clear();

    const auto &scene_binding = engine->render_state.GetScene();
    const auto scene_id = scene_binding.id;
    const auto scene_index = scene_binding ? scene_binding.id.value - 1 : 0;

    // check visibility state
    const bool perform_culling = scene_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.material_attributes.bucket);
    const auto visibility_cursor = engine->render_state.visibility_cursor;
    const auto &octree_visibility_state_snapshot = engine->GetWorld().GetOctree().GetVisibilityState().snapshots[visibility_cursor];

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
    Engine *engine,
    Frame *frame,
    const CullData &cull_data
)
{
    CollectDrawCalls(engine, frame);

    m_indirect_renderer.ExecuteCullShaderInBatches(
        engine,
        frame,
        cull_data
    );
}

static void GetDividedDrawCalls(
    const DynArray<EntityDrawProxy> &draw_proxies,
    UInt num_batches,
    DynArray<DynArray<EntityDrawProxy>> &out_divided_draw_proxies
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

template <bool IsIndirect>
static HYP_FORCE_INLINE void
RenderAll(
    Engine *engine,
    Frame *frame,
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> &command_buffers,
    UInt &command_buffer_index,
    renderer::GraphicsPipeline *pipeline,
    IndirectRenderer *indirect_renderer,
    const DynArray<EntityDrawProxy> &draw_proxies,
    DynArray<DynArray<EntityDrawProxy>> &divided_draw_proxies
)
{
    const auto &scene_binding = engine->render_state.GetScene();
    const auto scene_id = scene_binding.id;

    auto *instance = engine->GetInstance();
    auto *device = instance->GetDevice();

    const auto frame_index = frame->GetFrameIndex();

    const auto num_batches = RendererInstance::parallel_rendering
        ? MathUtil::Min(static_cast<UInt>(engine->task_system.GetPool(TaskPriority::HIGH).threads.Size()), num_async_rendering_command_buffers)
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
    engine->task_system.ParallelForEach(
        TaskPriority::HIGH,
        num_batches,
        divided_draw_proxies,
        [engine, pipeline, indirect_renderer, &command_buffers, &command_buffers_recorded_states, command_buffer_index, frame_index, scene_id](const DynArray<EntityDrawProxy> &draw_proxies, UInt index, UInt) {
            if (draw_proxies.Empty()) {
                return;
            }

            command_buffers[frame_index][index/*(command_buffer_index + batch_index) % static_cast<UInt>(command_buffers.Size())*/]->Record(
                engine->GetDevice(),
                pipeline->GetConstructionInfo().render_pass,
                [&](CommandBuffer *secondary) {
                    pipeline->Bind(secondary);

                    secondary->BindDescriptorSets(
                        engine->GetInstance()->GetDescriptorPool(),
                        pipeline,
                        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
                        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
                        FixedArray {
                            UInt32((scene_id.value - 1) * sizeof(SceneShaderData)),
                            HYP_RENDER_OBJECT_OFFSET(Light, 0)
                        }
                    );

#if HYP_FEATURES_BINDLESS_TEXTURES
                    /* Bindless textures */
                    engine->GetInstance()->GetDescriptorPool().Bind(
                        engine->GetDevice(),
                        secondary,
                        pipeline,
                        {
                            {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                        }
                    );
#endif
                    
                    engine->GetInstance()->GetDescriptorPool().Bind(
                        engine->GetDevice(),
                        secondary,
                        pipeline,
                        {
                            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
                        }
                    );
                    
                    for (const auto &draw_proxy : draw_proxies) {
                        const UInt entity_index = draw_proxy.entity_id != Entity::empty_id
                            ? draw_proxy.entity_id.value - 1
                            : 0;

                        const UInt skeleton_index = draw_proxy.skeleton_id != Skeleton::empty_id
                            ? draw_proxy.skeleton_id.value - 1
                            : 0;

                        const UInt material_index = draw_proxy.material_id != Material::empty_id
                            ? draw_proxy.material_id.value - 1
                            : 0;

#if HYP_FEATURES_BINDLESS_TEXTURES
                        secondary->BindDescriptorSets(
                            engine->GetInstance()->GetDescriptorPool(),
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
                        secondary->BindDescriptorSets(
                            engine->GetInstance()->GetDescriptorPool(),
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

                        if constexpr (IsIndirect) {
#if HYP_DEBUG_MODE
                            AssertThrow(draw_proxy.draw_command_index * sizeof(IndirectDrawCommand) < indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index)->size);
#endif
                            
                            draw_proxy.mesh->RenderIndirect(
                                engine,
                                secondary,
                                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                                draw_proxy.draw_command_index * sizeof(IndirectDrawCommand)
                            );
                        } else {
                            draw_proxy.mesh->Render(engine, secondary);
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

void RendererInstance::PerformRendering(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    RenderAll<false>(
        engine,
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline.get(),
        &m_indirect_renderer,
        m_indirect_renderer.GetDrawState().GetDrawProxies(),
        m_divided_draw_proxies
    );
}

void RendererInstance::PerformRenderingIndirect(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    RenderAll<true>(
        engine,
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline.get(),
        &m_indirect_renderer,
        m_indirect_renderer.GetDrawState().GetDrawProxies(),
        m_divided_draw_proxies
    );
}

void RendererInstance::Render(Engine *engine, Frame *frame)
{
    // perform all ops in one batch
    CollectDrawCalls(engine, frame);
    PerformRendering(engine, frame);
}

} // namespace hyperion::v2
