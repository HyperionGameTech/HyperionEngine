#include "Renderer.hpp"
#include <Engine.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

namespace hyperion::v2 {

RendererInstance::RendererInstance(
    Ref<Shader> &&shader,
    Ref<RenderPass> &&render_pass,
    const RenderableAttributeSet &renderable_attributes
) : EngineComponentBase(),
    m_pipeline(std::make_unique<renderer::GraphicsPipeline>()),
    m_shader(std::move(shader)),
    m_render_pass(std::move(render_pass)),
    m_renderable_attributes(renderable_attributes),
    m_per_frame_data(nullptr),
    m_multiview_index(~0u)
{
}

RendererInstance::~RendererInstance()
{
    Teardown();
}

void RendererInstance::RemoveFramebuffer(Framebuffer::ID id)
{
    const auto it = std::find_if(
        m_fbos.begin(),
        m_fbos.end(),
        [&](const auto &item) {
            return item->GetId() == id;
        }
    );

    if (it == m_fbos.end()) {
        return;
    }
    
    m_fbos.erase(it);
}

void RendererInstance::AddEntity(Ref<Entity> &&entity)
{
    AssertThrow(entity != nullptr);
    // AssertThrow(entity->IsReady());

    // AssertThrow(entity->IsRenderable());

    // FIXME: thread safety. this could be called from any thread
    AssertThrowMsg(
        (m_renderable_attributes.vertex_attributes & entity->GetRenderableAttributes().vertex_attributes) == entity->GetRenderableAttributes().vertex_attributes,
        "Pipeline vertex attributes does not satisfy the required vertex attributes of the entity."
    );

    // if (IsInitCalled()) {
    //     entity->Init(GetEngine());
    // }

    entity->OnAddedToPipeline(this);
    
    std::lock_guard guard(m_enqueued_entities_mutex);

    // remove from 'pending removal' list
    auto it = std::find(
        m_entities_pending_removal.begin(),
        m_entities_pending_removal.end(),
        entity
    );

    if (it != m_entities_pending_removal.end()) {
        m_entities_pending_removal.erase(it);
    }
    
    m_entities_pending_addition.push_back(std::move(entity));
    
    UpdateEnqueuedEntitiesFlag();
}

void RendererInstance::RemoveEntity(Ref<Entity> &&entity, bool call_on_removed)
{
    // TODO:: Make all RendererInstance operations operate in the RENDER
    // thread. Deferred rendering will be some derived version of a RenderComponent, so
    // it will acquire entities that way and hand them off from the render thread here.

    // we cannot touch m_entities from any thread other than the render thread
    // we'll have to assume it's here, and check later :/ 

    // auto entities_it = std::find_if(
    //     m_entities.begin(),
    //     m_entities.end(),
    //     [&id](const auto &item) {
    //         AssertThrow(item != nullptr);
    //         return item->GetId() == id;
    //     }
    // );

    // if (entities_it != m_entities.end()) {
    //     auto &&found_entity = *entities_it;


        // add it to pending removal list
        std::lock_guard guard(m_enqueued_entities_mutex);

        const auto pending_removal_it = std::find(
            m_entities_pending_removal.begin(),
            m_entities_pending_removal.end(),
            entity
        );

        if (pending_removal_it == m_entities_pending_removal.end()) {
            if (call_on_removed) {
                entity->OnRemovedFromPipeline(this);
            }

            auto pending_addition_it = std::find(
                m_entities_pending_addition.begin(),
                m_entities_pending_addition.end(),
                entity
            );

            if (pending_addition_it != m_entities_pending_addition.end()) {
                
                // directly remove from list of ones pending addition
                m_entities_pending_addition.erase(pending_addition_it);
            } else {
                /*m_cached_render_data.push_back(CachedRenderData{
                    .cycles_remaining = max_frames_in_flight + 1,
                    .entity_id       = entity->GetId(),
                    .material = entity->GetMaterial() != nullptr
                        ? entity->GetMaterial().IncRef()
                        : nullptr,
                    .mesh     = entity->GetMesh() != nullptr
                        ? entity->GetMesh().IncRef()
                        : nullptr,
                    .skeleton = entity->GetSkeleton() != nullptr
                        ? entity->GetSkeleton().IncRef()
                        : nullptr,
                    .shader   = entity->GetShader() != nullptr
                        ? entity->GetShader().IncRef()
                        : nullptr
                });*/

                m_entities_pending_removal.push_back(std::move(entity));
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

        // return;
    // }

    // not found in main entities list, check pending addition/removal

    // std::lock_guard guard(m_enqueued_entities_mutex);


    
}

// void RendererInstance::OnEntityRemoved(Entity *entity)
// {
//     RemoveEntity(entity->GetId(), false);
// }

void RendererInstance::BuildDrawCommandsBuffer(Engine *engine, UInt frame_index)
{
}

void RendererInstance::PerformEnqueuedEntityUpdates(Engine *engine, UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    std::lock_guard guard(m_enqueued_entities_mutex);

    if (!m_entities_pending_removal.empty()) {
        for (auto it = m_entities_pending_removal.begin(); it != m_entities_pending_removal.end();) {
            const auto entities_it = std::find(m_entities.begin(), m_entities.end(), *it); // oof
            
            if (entities_it != m_entities.end()) {
                m_entities.erase(entities_it);
            }

            it = m_entities_pending_removal.erase(it);
        }
    }

    if (!m_entities_pending_addition.empty()) {
        // we only add entities that are fully ready,
        // keeping ones that aren't finished initializing in the vector,
        // they should be finished on the next pass
        
        auto it = m_entities_pending_addition.begin();
        
        while (it != m_entities_pending_addition.end()) {
            AssertThrow(*it != nullptr);

            if ((*it)->GetMesh() == nullptr // TODO: I don't believe it's threadsafe to just check if this is null like this
                || std::find(m_entities.begin(), m_entities.end(), *it) != m_entities.end()) { // :(
                it = m_entities_pending_addition.erase(it);
                
                continue;
            }
            
            if ((*it)->IsReady() && (*it)->GetMesh()->IsReady()) {
                m_entities.push_back(std::move(*it));
                it = m_entities_pending_addition.erase(it);
                
                continue;
            }
            
            // not ready, keep looping.
            ++it;
        }
    }

    UpdateEnqueuedEntitiesFlag();
}

void RendererInstance::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        auto *engine = GetEngine();

        AssertThrow(!m_fbos.empty());

        for (auto &fbo : m_fbos) {
            AssertThrow(fbo != nullptr);
            fbo.Init();
        }

        AssertThrow(m_shader != nullptr);
        m_shader.Init();

        for (auto &&entity : m_entities) {
            AssertThrow(entity != nullptr);

            entity->Init(engine);
        }

        m_indirect_renderer.Create(engine);

        engine->render_scheduler.Enqueue([this, engine](...) {
            renderer::GraphicsPipeline::ConstructionInfo construction_info {
                .vertex_attributes = m_renderable_attributes.vertex_attributes,
                .topology          = m_renderable_attributes.topology,
                .cull_mode         = m_renderable_attributes.cull_faces,
                .fill_mode         = m_renderable_attributes.fill_mode,
                .depth_test        = m_renderable_attributes.depth_test,
                .depth_write       = m_renderable_attributes.depth_write,
                .blend_enabled     = m_renderable_attributes.alpha_blending,
                .shader            = m_shader->GetShaderProgram(),
                .render_pass       = &m_render_pass->GetRenderPass(),
                .stencil_state     = m_renderable_attributes.stencil_state,
                .multiview_index   = m_multiview_index
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

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this](...) {
            auto *engine = GetEngine();

            SetReady(false);

            m_indirect_renderer.Destroy(engine);

            for (auto &&entity : m_entities) {
                AssertThrow(entity != nullptr);

                entity->OnRemovedFromPipeline(this);
            }

            m_entities.clear();
            
            std::lock_guard guard(m_enqueued_entities_mutex);

            for (auto &&entity : m_entities_pending_addition) {
                if (entity == nullptr) {
                    continue;
                }

                entity->OnRemovedFromPipeline(this);
            }

            m_entities_pending_addition.clear();
            m_entities_pending_removal.clear();

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
        }));
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
    const auto scene_id      = scene_binding.id;
    const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

    // check visibility state
    const bool perform_culling = scene_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.bucket);

    m_indirect_renderer.GetDrawState().ResetDrawProxies();
    //m_indirect_renderer.GetDrawState().Reserve(engine, frame, m_entities.size());

    for (auto &&entity : m_entities) {
        if (entity->GetMesh() == nullptr) {
            continue;
        }

        if (perform_culling) {
            if (auto *octant = entity->GetOctree()) {
                const auto &visibility_state = octant->GetVisibilityState();

                if (!Octree::IsVisible(&engine->GetWorld().GetOctree(), octant, engine->render_state.visibility_cursor)) {
                    continue;
                }

                if (!visibility_state.Get(scene_id)) {
                    continue;
                }
            } else {
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
    const auto scene_id      = scene_binding.id;

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
                FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,        DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
                FixedArray {
                    UInt32((scene_id.value - 1) * sizeof(SceneShaderData)),
                    UInt32(0           * sizeof(LightShaderData))
                }
            );

#if HYP_FEATURES_BINDLESS_TEXTURES
            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
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
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index] },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index   * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#else
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT,        DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index   * sizeof(ObjectShaderData)),
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

    /* Remove cache data. This data exists so that there is some buffer time
       in between removing an entity from the pipeline and the data (potentially)
       being destroyed. We use (max frames in flight) + 1 and count down each Render() call.
    */
    
    /*if (!m_cached_render_data.empty()) {
        auto it = m_cached_render_data.begin();

        while (it != m_cached_render_data.end()) {
            if (!--it->cycles_remaining) {
                it = m_cached_render_data.erase(it);
            } else {
                ++it;
            }
        }
    }*/

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
            const auto scene_cull_id = scene_binding.id; //scene_binding.parent_id ? scene_binding.parent_id : scene_binding.id;
            const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

            secondary->BindDescriptorSets(
                instance->GetDescriptorPool(),
                m_pipeline.get(),
                FixedArray { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
                FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,        DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
                FixedArray {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0           * sizeof(LightShaderData))
                }
            );

#if HYP_FEATURES_BINDLESS_TEXTURES
            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
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
            const bool perform_culling = scene_cull_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.bucket);
            
            for (auto &&entity : m_entities) {
                if (perform_culling) {
                    if (auto *octant = entity->GetOctree()) {
                        const auto &visibility_state = octant->GetVisibilityState();
                        
                        if (!Octree::IsVisible(&engine->GetWorld().GetOctree(), octant, engine->render_state.visibility_cursor)) {
                            ++num_culled_objects;
                            continue;
                        }

                        if (!visibility_state.Get(scene_cull_id)) {
                            ++num_culled_objects;
                            continue;
                        }
                    } else {
                        DebugLog(
                            LogType::Warn,
                            "In pipeline #%u: entity #%u not in octree!\n",
                            m_id.value,
                            entity->GetId().value
                        );

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
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index] },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index  * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#else
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT,        DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index  * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#endif

                entity->GetMesh()->Render(engine, secondary);
            }

            // DebugLog(
            //     LogType::Debug,
            //     "Scene %u: Culled %u objects\n",
            //     scene_cull_id.value,
            //     num_culled_objects
            // );

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
