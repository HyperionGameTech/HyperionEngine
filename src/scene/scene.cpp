#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/Environment.hpp>
#include <rendering/render_components/CubemapRenderer.hpp>

namespace hyperion::v2 {

Scene::Scene(std::unique_ptr<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera)),
      m_root_node(std::make_unique<Node>("root")),
      m_environment(new Environment(this)),
      m_world(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    m_root_node->SetScene(this);
}

Scene::~Scene()
{
    Teardown();

    delete m_environment;
}
    
void Scene::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SCENES, [this](Engine *engine) {
        m_environment->Init(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SCENES, [this](Engine *engine) {
            DebugLog(LogType::Debug, "Destroy scene #%lu\t%p\n", m_id.value, (void *)this);

            m_root_node->SetScene(nullptr);

            for (auto &spatial : m_spatials) {
                AssertThrow(spatial.second != nullptr);

                spatial.second->m_scene = nullptr;

                RemoveFromPipelines(spatial.second);

                if (spatial.second->m_octree != nullptr) {
                    spatial.second->RemoveFromOctree(GetEngine());
                }
            }

            m_spatials.clear();

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

void Scene::SetWorld(World *world)
{
    Threads::AssertOnThread(THREAD_GAME);

    for (auto &it : m_spatials) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        if (entity->m_octree != nullptr) {
            DebugLog(
                LogType::Debug,
                "[scene] Remove spatial #%u from octree\n",
                entity->GetId().value
            );
            entity->RemoveFromOctree(GetEngine());
        }
    }

    m_world = world;

    if (m_world == nullptr) {
        return;
    }

    for (auto &it : m_spatials) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        AssertThrow(entity->m_octree == nullptr);
        
        DebugLog(
            LogType::Debug,
            "Add spatial #%u to octree\n",
            entity->GetId().value
        );
        entity->AddToOctree(GetEngine(), m_world->GetOctree());
    }
}

bool Scene::AddSpatial(Ref<Spatial> &&spatial)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(spatial != nullptr);

    auto it = m_spatials.find(spatial->GetId());

    if (it != m_spatials.end()) {
        DebugLog(
            LogType::Warn,
            "Spatial #%u already exists in Scene #%u\n",
            spatial->GetId().value,
            m_id.value
        );

        return false;
    }

    spatial->m_scene = this;
    spatial.Init();

    if (spatial->IsRenderable() && !spatial->GetPrimaryPipeline()) {
        if (auto pipeline = GetEngine()->FindOrCreateGraphicsPipeline(spatial->GetRenderableAttributes())) {
            if (!spatial->GetPipelines().Contains(pipeline.ptr)) {
                pipeline->AddSpatial(spatial.IncRef());

                spatial->EnqueueRenderUpdates(GetEngine());
            }

            // spatial->SetShaderDataState(ShaderDataState::DIRTY);
            
            // TODO: wrapper function
            spatial->m_primary_pipeline = {
                .pipeline = pipeline.ptr,
                .changed  = false
            };
        } else {
            DebugLog(
                LogType::Error,
                "Could not find or create optimal graphics pipeline for Spatial #%lu!\n",
                spatial->GetId().value
            );

            return false;
        }
    }

    if (spatial->m_octree == nullptr) {
        if (m_world != nullptr) {
            DebugLog(
                LogType::Debug,
                "Add spatial #%u to octree\n",
                spatial->GetId().value
            );
            spatial->AddToOctree(GetEngine(), m_world->GetOctree());
        }
    }

    m_environment->OnEntityAdded(spatial);

    // m_spatials.Insert(spatial->GetId(), std::move(spatial));
    m_spatials.insert(std::make_pair(static_cast<IDBase>(spatial->GetId()), std::move(spatial)));

    return true;
}

bool Scene::HasSpatial(Spatial::ID id) const
{
    Threads::AssertOnThread(THREAD_GAME);

    return m_spatials.find(id) != m_spatials.end();
}

bool Scene::RemoveSpatial(Spatial::ID id)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    auto it = m_spatials.find(id);
    if (it == m_spatials.end()) {
        return false;
    }

    auto &found_spatial = it->second;
    AssertThrow(found_spatial != nullptr);

    m_environment->OnEntityRemoved(found_spatial);

    RemoveFromPipelines(found_spatial);

    if (found_spatial->m_octree != nullptr) {
        DebugLog(
            LogType::Debug,
            "[scene] Remove spatial #%u from octree\n",
            found_spatial->GetId().value
        );
        found_spatial->RemoveFromOctree(GetEngine());
    }

    m_spatials.erase(it);

    return true;
}

bool Scene::RemoveSpatial(const Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(spatial != nullptr);

    auto it = m_spatials.find(spatial->GetId());
    if (it == m_spatials.end()) {
        return false;
    }

    auto &found_spatial = it->second;
    AssertThrow(found_spatial != nullptr);

    RemoveFromPipelines(found_spatial);

    if (found_spatial->m_octree != nullptr) {
        found_spatial->RemoveFromOctree(GetEngine());
    }

    m_spatials.erase(it);

    return true;
}

void Scene::Update(
    Engine *engine,
    GameCounter::TickUnit delta,
    bool update_octree_visibility
)
{
    AssertReady();

    m_environment->Update(engine, delta);

    if (m_camera != nullptr) {
        m_camera->Update(delta);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    EnqueueRenderUpdates(engine);

    for (auto &it : m_spatials) {
        auto &spatial = it.second;
        AssertThrow(spatial != nullptr);

        spatial->Update(engine, delta);

        if (spatial->m_primary_pipeline.changed) {
            RequestPipelineChanges(spatial);
        }
    }

    if (m_world != nullptr && update_octree_visibility) {
        // m_octree.NextVisibilityState();
        m_world->GetOctree().CalculateVisibility(this);
    }
}

void Scene::RequestPipelineChanges(Ref<Spatial> &spatial)
{
    AssertThrow(spatial != nullptr);
    AssertThrow(spatial->m_scene == this);

    if (spatial->m_primary_pipeline.pipeline != nullptr) {
        RemoveFromPipeline(spatial, spatial->m_primary_pipeline.pipeline);
    }

    if (!spatial->GetPrimaryPipeline()) {
        if (auto pipeline = GetEngine()->FindOrCreateGraphicsPipeline(spatial->GetRenderableAttributes())) {
            if (!spatial->GetPipelines().Contains(pipeline.ptr)) {
                pipeline->AddSpatial(spatial.IncRef());
            }
            
            // TODO: wrapper function
            spatial->m_primary_pipeline = {
                .pipeline = pipeline.ptr,
                .changed  = false
            };
        } else {
            DebugLog(
                LogType::Error,
                "Could not find or create optimal graphics pipeline for Spatial #%lu!\n",
                spatial->GetId().value
            );
        }
    }
}

void Scene::RemoveFromPipeline(Ref<Spatial> &spatial, GraphicsPipeline *pipeline)
{
    // if (pipeline == spatial->m_primary_pipeline.pipeline) {
    //     spatial->m_primary_pipeline = {
    //         .pipeline = nullptr,
    //         .changed  = true
    //     };
    // }

    pipeline->RemoveSpatial(spatial.IncRef());
    // spatial->OnRemovedFromPipeline(pipeline);
}

void Scene::RemoveFromPipelines(Ref<Spatial> &spatial)
{
    auto pipelines = spatial->m_pipelines;

    for (auto *pipeline : pipelines) {
        if (pipeline == nullptr) {
            continue;
        }

        // have to inc ref so it can hold it in a temporary container
        pipeline->RemoveSpatial(spatial.IncRef());
    }

    // spatial->m_pipelines.Clear();
    // spatial->m_primary_pipeline = {
    //     .pipeline = nullptr,
    //     .changed  = true
    // };
}

void Scene::EnqueueRenderUpdates(Engine *engine)
{
    struct {
        BoundingBox aabb;
        Matrix4     view;
        Matrix4     projection;
        Vector3     translation;
        Vector3     direction;
        Int         width;
        Int         height;
        Vector3     light_direction;
        Float       global_timer;
        Float       camera_near;
        Float       camera_far;
    } params = {
        .aabb            = m_aabb,
        .global_timer    = m_environment->GetGlobalTimer()
    };

    if (m_camera != nullptr) {
        params.view        = m_camera->GetViewMatrix();
        params.projection  = m_camera->GetProjectionMatrix();
        params.translation = m_camera->GetTranslation();
        params.direction   = m_camera->GetDirection();
        params.width       = m_camera->GetWidth();
        params.height      = m_camera->GetHeight();
        params.camera_near = m_camera->GetNear();
        params.camera_far  = m_camera->GetFar();
    }

    engine->render_scheduler.Enqueue([this, engine, params](...) {
        SceneShaderData shader_data{
            .view                        = params.view,
            .projection                  = params.projection,
            .camera_position             = params.translation.ToVector4(),
            .camera_direction            = params.direction.ToVector4(),
            .light_direction             = params.light_direction.ToVector4(),
            .resolution_x                = static_cast<UInt32>(params.width),
            .resolution_y                = static_cast<UInt32>(params.height),
            .aabb_max                    = params.aabb.max.ToVector4(),
            .aabb_min                    = params.aabb.min.ToVector4(),
            .global_timer                = params.global_timer,
            .num_environment_shadow_maps = static_cast<UInt32>(m_environment->HasRenderComponent<ShadowRenderer>()), // callable on render thread only
            .num_lights                  = static_cast<UInt32>(m_environment->NumLights()),
            .camera_near                 = params.camera_near,
            .camera_far                  = params.camera_far
        };

        shader_data.environment_texture_usage = 0;

        //! TODO: allow any cubemap to be set on env
        if (const auto *cubemap_renderer = m_environment->GetRenderComponent<CubemapRenderer>()) {
            shader_data.environment_texture_index = cubemap_renderer->GetComponentIndex();
            shader_data.environment_texture_usage |= 1u << cubemap_renderer->GetComponentIndex();
        }

        // DebugLog(LogType::Debug, "set %u lights\n", shader_data.num_lights);
        
        engine->shader_globals->scenes.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

} // namespace hyperion::v2
