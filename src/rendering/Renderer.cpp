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
    m_renderable_attributes(renderable_attributes),
    m_per_frame_data(nullptr)
{
}

RendererInstance::~RendererInstance()
{
    Teardown();
}

void RendererInstance::RemoveFramebuffer(Framebuffer::ID id)
{
    const auto it = m_fbos.FindIf([&](const auto &item) {
        return item->GetId() == id;
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
            entity->GetId().value,
            GetId().value
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

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        auto *engine = GetEngine();

        engine->render_scheduler.Enqueue([this, engine](...) {
            renderer::GraphicsPipeline::ConstructionInfo construction_info {
                .vertex_attributes = m_renderable_attributes.mesh_attributes.vertex_attributes,
                .topology          = m_renderable_attributes.mesh_attributes.topology,
                .cull_mode         = m_renderable_attributes.mesh_attributes.cull_faces,
                .fill_mode         = m_renderable_attributes.mesh_attributes.fill_mode,
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

            AssertThrow(m_per_frame_data == nullptr);
            m_per_frame_data = new PerFrameData<CommandBuffer>(engine->GetInstance()->GetFrameHandler()->NumFrames());

            auto &per_frame_data = *m_per_frame_data;

            for (UInt32 i = 0; i < per_frame_data.NumFrames(); i++) {
                auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY);
                HYPERION_BUBBLE_ERRORS(command_buffer->Create(
                    engine->GetInstance()->GetDevice(),
                    engine->GetInstance()->GetGraphicsCommandPool()
                ));

                per_frame_data[i].Set<CommandBuffer>(std::move(command_buffer));
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

            engine->render_scheduler.Enqueue([this, engine](...) {
                if (m_per_frame_data != nullptr) {
                    auto &per_frame_data = *m_per_frame_data;

                    for (UInt i = 0; i < per_frame_data.NumFrames(); i++) {
                        HYPERION_BUBBLE_ERRORS(per_frame_data[i].Get<CommandBuffer>()->Destroy(
                            engine->GetInstance()->GetDevice(),
                            engine->GetInstance()->GetGraphicsCommandPool()
                        ));
                    }

                    delete m_per_frame_data;
                    m_per_frame_data = nullptr;
                }

                return m_pipeline->Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        });
    }));
}

void RendererInstance::CollectDrawCalls(
    Engine *engine,
    Frame *frame,
    const CullData &cull_data
)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (m_enqueued_entities_flag.load()) {
        PerformEnqueuedEntityUpdates(engine, frame->GetFrameIndex());
    }

    const auto scene_binding = engine->render_state.GetScene();
    const auto scene_id = scene_binding.id;
    const auto scene_index = scene_binding ? scene_binding.id.value - 1 : 0;

    // check visibility state
    const bool perform_culling = scene_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.material_attributes.bucket);
    const auto visibility_cursor = engine->render_state.visibility_cursor;
    const auto &octree_visibility_state_snapshot = engine->GetWorld().GetOctree().GetVisibilityState().snapshots[visibility_cursor];

    m_indirect_renderer.GetDrawState().ResetDrawProxies();
    //m_indirect_renderer.GetDrawState().Reserve(engine, frame, m_entities.size());

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

    m_indirect_renderer.ExecuteCullShaderInBatches(
        engine,
        frame,
        cull_data
    );
}

void RendererInstance::PerformRendering(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(m_per_frame_data != nullptr);

    const auto scene_binding = engine->render_state.GetScene();
    const auto scene_id = scene_binding.id;

    auto *instance = engine->GetInstance();
    auto *device = instance->GetDevice();
    auto *secondary_command_buffer = m_per_frame_data->At(frame->GetFrameIndex()).Get<CommandBuffer>();

    secondary_command_buffer->Record(
        device,
        m_pipeline->GetConstructionInfo().render_pass,
        [this, engine, instance, device, scene_id, frame_index = frame->GetFrameIndex()](CommandBuffer *secondary) {    
            m_pipeline->Bind(secondary);

            secondary->BindDescriptorSets(
                instance->GetDescriptorPool(),
                m_pipeline.get(),
                FixedArray { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
                FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
                FixedArray {
                    UInt32((scene_id.value - 1) * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData))
                }
            );

#if HYP_FEATURES_BINDLESS_TEXTURES
            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_renderer_instance.get(),
                {
                    {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                }
            );
#endif
            
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
                }
            );

            for (auto &draw_proxy : m_indirect_renderer.GetDrawState().GetDrawProxies()) {
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
                    instance->GetDescriptorPool(),
                    m_renderer_instance.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index] },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#else
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT, DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#endif

                draw_proxy.mesh->RenderIndirect(
                    engine,
                    secondary,
                    m_indirect_renderer.GetDrawState().GetIndirectBuffer(frame_index),
                    draw_proxy.draw_command_index * sizeof(IndirectDrawCommand)
                );
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}


void RendererInstance::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    AssertThrow(m_per_frame_data != nullptr);

    if (m_enqueued_entities_flag.load()) {
        PerformEnqueuedEntityUpdates(engine, frame->GetFrameIndex());
    }

    auto *instance = engine->GetInstance();
    auto *device = instance->GetDevice();
    auto *secondary_command_buffer = m_per_frame_data->At(frame->GetFrameIndex()).Get<CommandBuffer>();

    secondary_command_buffer->Record(
        device,
        m_pipeline->GetConstructionInfo().render_pass,
        [this, engine, instance, device, frame_index = frame->GetFrameIndex()](CommandBuffer *secondary) {    
            UInt num_culled_objects = 0;
   
            m_pipeline->Bind(secondary);

            static_assert(std::size(DescriptorSet::object_buffer_mapping) == max_frames_in_flight);

            const auto scene_binding = engine->render_state.GetScene();
            const auto scene_id = scene_binding.id;
            const auto scene_index = scene_binding ? scene_binding.id.value - 1 : 0;

            secondary->BindDescriptorSets(
                instance->GetDescriptorPool(),
                m_pipeline.get(),
                FixedArray { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
                FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
                FixedArray {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData))
                }
            );

#if HYP_FEATURES_BINDLESS_TEXTURES
            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_renderer_instance.get(),
                {
                    {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                }
            );
#endif
            
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
                }
            );

            // check visibility state
            const bool perform_culling = scene_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.material_attributes.bucket);
            const auto visibility_cursor = engine->render_state.visibility_cursor;
            const auto &octree_visibility_state_snapshot = engine->GetWorld().GetOctree().GetVisibilityState().snapshots[visibility_cursor];

            for (auto &&entity : m_entities) {
                if (perform_culling) {
                    const auto &snapshot = entity->GetVisibilityState().snapshots[visibility_cursor];

                    if (!snapshot.ValidToParent(octree_visibility_state_snapshot)) {
                        continue;
                    }

                    if (!snapshot.Get(scene_id)) {
                        continue;
                    }
                }

                const auto entity_index = entity->GetId().value - 1;

                UInt material_index = 0;

                if (entity->GetMaterial() != nullptr && entity->GetMaterial()->IsReady()) {
                    // TODO: rather than checking each call we should just add once
                    material_index = entity->GetMaterial()->GetId().value - 1;
                }

                const auto skeleton_index = entity->GetSkeleton() != nullptr
                    ? entity->GetSkeleton()->GetId().value - 1
                    : 0;

#if HYP_FEATURES_BINDLESS_TEXTURES
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_renderer_instance.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index] },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#else
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT, DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#endif

                entity->GetMesh()->Render(engine, secondary);
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
