#include "graphics.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor_set.h>

#include "rendering/backend/renderer_graphics_pipeline.h"

namespace hyperion::v2 {

bool GraphicsPipeline::BucketSupportsCulling(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT
        || bucket == BUCKET_PARTICLE;
}

GraphicsPipeline::GraphicsPipeline(
    Ref<Shader> &&shader,
    Ref<RenderPass> &&render_pass,
    const RenderableAttributeSet &renderable_attributes
) : EngineComponentBase(),
    m_pipeline(std::make_unique<renderer::GraphicsPipeline>()),
    m_shader(std::move(shader)),
    m_render_pass(std::move(render_pass)),
    m_renderable_attributes(renderable_attributes),
    m_per_frame_data(nullptr),
    m_spatial_notifier([this]() -> std::vector<std::pair<Ref<Spatial> *, size_t>> {
        return {
            std::make_pair(m_spatials.data(), m_spatials.size()),
            std::make_pair(m_spatials_pending_addition.data(), m_spatials_pending_addition.size())
        };
    })
{
}

GraphicsPipeline::~GraphicsPipeline()
{
    Teardown();
}

void GraphicsPipeline::RemoveFramebuffer(Framebuffer::ID id)
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

void GraphicsPipeline::AddSpatial(Ref<Spatial> &&spatial)
{
    AssertThrow(spatial != nullptr);
    AssertThrowMsg(
        (m_renderable_attributes.vertex_attributes & spatial->GetRenderableAttributes().vertex_attributes) == spatial->GetRenderableAttributes().vertex_attributes,
        "Pipeline vertex attributes does not satisfy the required vertex attributes of the spatial."
    );

    spatial.Init();
    spatial->OnAddedToPipeline(this);
    
    std::lock_guard guard(m_enqueued_spatials_mutex);
    
    m_spatial_notifier.ItemAdded(spatial);
    m_spatials_pending_addition.push_back(std::move(spatial));
}

bool GraphicsPipeline::RemoveFromSpatialList(
    Spatial::ID id,
    std::vector<Ref<Spatial>> &spatials,
    bool call_on_removed,
    bool dispatch_item_removed,
    bool remove_immediately
)
{
    const auto it = std::find_if(
        spatials.begin(),
        spatials.end(),
        [&id](const auto &item) {
            return item->GetId() == id;
        }
    );

    if (it == spatials.end()) {
        return false;
    }

    auto &found_spatial = *it;

    if (found_spatial != nullptr) {
        if (dispatch_item_removed) {
            m_spatial_notifier.ItemRemoved(found_spatial);
        }

        if (call_on_removed) {
            found_spatial->OnRemovedFromPipeline(this);
        }
    }

    if (remove_immediately) {
        spatials.erase(it);
    } else {
        std::lock_guard guard(m_enqueued_spatials_mutex);

        m_spatials_pending_removal.push_back(it->IncRef());
    }
    
    return true;
}

void GraphicsPipeline::RemoveSpatial(Spatial::ID id)
{
    if (RemoveFromSpatialList(id, m_spatials, true, true, false)) {
        return;
    }

    std::lock_guard guard(m_enqueued_spatials_mutex);

    if (RemoveFromSpatialList(id, m_spatials_pending_addition, true, true, true)) {
        return;
    }

    RemoveFromSpatialList(id, m_spatials_pending_removal, false, false, true);
}

void GraphicsPipeline::OnSpatialRemoved(Spatial *spatial)
{
    const auto id = spatial->GetId();

    if (RemoveFromSpatialList(id, m_spatials, false, true, false)) {
        return;
    }

    std::lock_guard guard(m_enqueued_spatials_mutex);

    if (RemoveFromSpatialList(id, m_spatials_pending_addition, false, true, true)) {
        return;
    }

    RemoveFromSpatialList(id, m_spatials_pending_removal, false, false, true);
}

void GraphicsPipeline::PerformEnqueuedSpatialUpdates(Engine *engine)
{
    Engine::AssertOnThread(THREAD_RENDER);

    std::lock_guard guard(m_enqueued_spatials_mutex);

    if (!m_spatials_pending_removal.empty()) {
        for (auto &spatial : m_spatials_pending_removal) {
            const auto it = std::find(m_spatials.begin(), m_spatials.end(), spatial.ptr); // oof

            if (it != m_spatials.end()) {
                m_spatials.erase(it);
            }
        }

        m_spatials_pending_removal.clear();
    }

    if (!m_spatials_pending_addition.empty()) {
        m_spatials.insert(
            m_spatials.end(),
            std::make_move_iterator(m_spatials_pending_addition.begin()), 
            std::make_move_iterator(m_spatials_pending_addition.end())
        );

        m_spatials_pending_addition.clear();
    }
}

void GraphicsPipeline::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](Engine *engine) {
        AssertThrow(!m_fbos.empty());

        for (auto &fbo : m_fbos) {
            AssertThrow(fbo != nullptr);
            fbo.Init();
        }

        AssertThrow(m_shader != nullptr);
        m_shader.Init();

        engine->render_scheduler.Enqueue([this, engine](...) {
            renderer::GraphicsPipeline::ConstructionInfo construction_info{
                .vertex_attributes = m_renderable_attributes.vertex_attributes,
                .topology          = m_renderable_attributes.topology,
                .cull_mode         = m_renderable_attributes.cull_faces,
                .fill_mode         = m_renderable_attributes.fill_mode,
                .depth_test        = m_renderable_attributes.depth_test,
                .depth_write       = m_renderable_attributes.depth_write,
                .blend_enabled     = m_renderable_attributes.alpha_blending,
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

            for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
                auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY);
                HYPERION_BUBBLE_ERRORS(command_buffer->Create(
                    engine->GetInstance()->GetDevice(),
                    engine->GetInstance()->GetGraphicsCommandPool()
                ));

                per_frame_data[i].Set<CommandBuffer>(std::move(command_buffer));
            }

            return m_pipeline->Create(
                engine->GetDevice(),
                std::move(construction_info),
                &engine->GetInstance()->GetDescriptorPool()
            );
        });

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this](Engine *engine) {
            for (auto &spatial : m_spatials) {
                if (spatial == nullptr) {
                    continue;
                }

                m_spatial_notifier.ItemRemoved(spatial);

                spatial->OnRemovedFromPipeline(this);
            }

            m_spatials.clear();
            
            std::lock_guard guard(m_enqueued_spatials_mutex);

            for (auto &spatial : m_spatials_pending_addition) {
                if (spatial == nullptr) {
                    continue;
                }

                m_spatial_notifier.ItemRemoved(spatial);

                spatial->OnRemovedFromPipeline(this);
            }

            m_spatials_pending_addition.clear();
            m_spatials_pending_removal.clear();

            engine->render_scheduler.Enqueue([this, engine](...) {
                if (m_per_frame_data != nullptr) {
                    auto &per_frame_data = *m_per_frame_data;

                    for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
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

            SetReady(false);
        }), engine);
    }));
}

void GraphicsPipeline::Render(Engine *engine, Frame *frame)
{
    Engine::AssertOnThread(THREAD_RENDER);

    AssertReady();

    AssertThrow(m_per_frame_data != nullptr);

    if (!m_spatials_pending_addition.empty() || !m_spatials_pending_removal.empty()) {
        PerformEnqueuedSpatialUpdates(engine);
    }

    auto *instance = engine->GetInstance();
    auto *device = instance->GetDevice();
    auto *secondary_command_buffer = m_per_frame_data->At(frame->GetFrameIndex()).Get<CommandBuffer>();

    secondary_command_buffer->Record(
        device,
        m_pipeline->GetConstructionInfo().render_pass,
        [this, engine, instance, device, frame_index = frame->GetFrameIndex()](CommandBuffer *secondary) {
            m_pipeline->Bind(secondary);

            /* Bind global data */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {{
                    .set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,
                    .count = 1
                }}
            );

            static_assert(std::size(DescriptorSet::object_buffer_mapping) == Swapchain::max_frames_in_flight);

            const auto scene_binding = engine->render_state.GetScene();
            const auto scene_cull_id = scene_binding.parent_id ? scene_binding.parent_id : scene_binding.id;
            const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

            /* Bind scene data - */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::scene_buffer_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                    {.offsets = {uint32_t(scene_index * sizeof(SceneShaderData))}}
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
            const bool perform_culling = scene_cull_id != Scene::empty_id && BucketSupportsCulling(m_renderable_attributes.bucket);
            
            for (auto &spatial : m_spatials) {
                if (spatial->GetMesh() == nullptr) {
                    continue;
                }

                if (perform_culling) {
                    auto *octant = spatial->GetOctree();

                    if (auto *octant = spatial->GetOctree()) {
                        const auto &visibility_state = octant->GetVisibilityState();

                        if (!Octree::IsVisible(&engine->GetOctree(), octant)) {
                            continue;
                        }

                        if (!visibility_state.Get(scene_cull_id)) {
                            continue;
                        }
                    } else {
                        DebugLog(
                            LogType::Warn,
                            "Spatial #%lu not in octree!\n",
                            spatial->GetId().value
                        );

                        continue;
                    }
                }

                const auto spatial_index = spatial->GetId().value - 1;

                const auto material_index = spatial->GetMaterial() != nullptr
                    ? spatial->GetMaterial()->GetId().value - 1
                    : 0;

                const auto skeleton_index = spatial->GetSkeleton() != nullptr
                    ? spatial->GetSkeleton()->GetId().value - 1
                    : 0;

                /* Bind per-object / material data separately */
                instance->GetDescriptorPool().Bind(
                    device,
                    secondary,
                    m_pipeline.get(),
                    {
                        {.set = DescriptorSet::object_buffer_mapping[frame_index], .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT},
                        {.offsets = {
                            uint32_t(material_index * sizeof(MaterialShaderData)),
                            uint32_t(spatial_index  * sizeof(ObjectShaderData)),
                            uint32_t(skeleton_index * sizeof(SkeletonShaderData))
                        }}
                    }
                );

#if !HYP_FEATURES_BINDLESS_TEXTURES
            /* Per-material texture set */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index), .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES}
                }
            );
#endif

                spatial->GetMesh()->Render(engine, secondary);
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
