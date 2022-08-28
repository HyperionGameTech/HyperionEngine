#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/render_components/CubemapRenderer.hpp>

namespace hyperion::v2 {

Scene::Scene(Handle<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera)),
      m_root_node_proxy(new Node("root")),
      m_environment(new RenderEnvironment(this)),
      m_world(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    m_root_node_proxy.Get()->SetScene(this);
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

    engine->InitObject(m_camera);

    for (auto &it : m_entities) {
        auto &entity = it.second;
        AssertThrow(entity);

        engine->InitObject(entity);
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SCENES, [this](...) {
        auto *engine = GetEngine();

        // if (m_camera) {
        //     engine->InitObject(m_camera);
        // }

        m_environment->Init(engine);

        SetReady(true);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            m_camera.Reset();

            m_root_node_proxy.Get()->SetScene(nullptr);

            for (auto &it : m_entities) {
                AssertThrow(it.second != nullptr);

                it.second->SetScene(nullptr);

                RemoveFromRendererInstances(it.second);

                if (it.second->m_octree != nullptr) {
                    it.second->RemoveFromOctree(GetEngine());
                }
            }

            m_entities.Clear();
            m_entities_pending_addition.Clear();
            m_entities_pending_removal.Clear();

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        });
    }));
}

void Scene::SetWorld(World *world)
{
    // be cautious
    // Threads::AssertOnThread(THREAD_GAME);

    for (auto &it : m_entities) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        if (entity->m_octree != nullptr) {
            DebugLog(
                LogType::Debug,
                "[scene] Remove entity #%u from octree\n",
                entity->GetId().value
            );

            entity->RemoveFromOctree(GetEngine());
        }
    }

    m_world = world;

    if (m_world == nullptr) {
        return;
    }

    for (auto &it : m_entities) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        AssertThrow(entity->m_octree == nullptr);

#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Debug,
            "Add entity #%u to octree\n",
            entity->GetId().value
        );
#endif

        entity->AddToOctree(GetEngine(), m_world->GetOctree());
    }
}

bool Scene::AddEntity(Handle<Entity> &&entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(entity != nullptr);

    if (m_entities.Contains(entity->GetId())) {
        DebugLog(
            LogType::Warn,
            "Entity #%u already exists in Scene #%u\n",
            entity->GetId().value,
            m_id.value
        );

        return false;
    }

    if (IsInitCalled()) {
        GetEngine()->InitObject(entity);
    }

    entity->SetScene(this);

    m_environment->OnEntityAdded(entity);

    m_entities_pending_addition.Insert(std::move(entity));

    return true;
}

bool Scene::HasEntity(Entity::ID id) const
{
    Threads::AssertOnThread(THREAD_GAME);

    return m_entities.Find(id) != m_entities.End();
}

bool Scene::RemoveEntity(const Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(entity != nullptr);

    auto it = m_entities.Find(entity->GetId());

    if (it == m_entities.End()) {
        DebugLog(
            LogType::Warn,
            "Could not remove entity with id #%u: not found\n",
            entity->GetId().value
        );

        return false;
    }

    if (m_entities_pending_removal.Contains(entity->GetId())) {
        DebugLog(
            LogType::Warn,
            "Could not remove entity with id #%u: already pending removal\n",
            entity->GetId().value
        );

        return false;
    }

    m_environment->OnEntityRemoved(it->second);
    m_entities_pending_removal.Insert(entity->GetId());

    return true;
}

void Scene::AddPendingEntities()
{
    if (m_entities_pending_addition.Empty()) {
        return;
    }

    for (auto &entity : m_entities_pending_addition) {
        const auto id = entity->GetId();

        if (entity->IsRenderable() && !entity->GetPrimaryRendererInstance()) {
            if (auto renderer_instance = GetEngine()->FindOrCreateRendererInstance(entity->GetShader(), entity->GetRenderableAttributes())) {
                renderer_instance->AddEntity(Handle<Entity>(entity));
                entity->EnqueueRenderUpdates();

                entity->m_primary_renderer_instance = {
                    .renderer_instance = renderer_instance.Get(),
                    .changed = false
                };
            } else {
                DebugLog(
                    LogType::Error,
                    "Could not find or create optimal RendererInstance for Entity #%lu!\n",
                    entity->GetId().value
                );

                continue;
            }
        }

        if (entity->m_octree == nullptr) {
            if (m_world != nullptr) {
#if HYP_OCTREE_DEBUG
                DebugLog(
                    LogType::Debug,
                    "Add entity #%u to octree\n",
                    entity->GetId().value
                );
#endif

                entity->AddToOctree(GetEngine(), m_world->GetOctree());
            }
        }

        m_entities.Insert(static_cast<IDBase>(id), std::move(entity));
    }

    m_entities_pending_addition.Clear();
}

void Scene::RemovePendingEntities()
{
    if (m_entities_pending_removal.Empty()) {
        return;
    }

    for (auto &id : m_entities_pending_removal) {
        auto it = m_entities.Find(id);

        auto &found_entity = it->second;
        AssertThrow(found_entity != nullptr);

        RemoveFromRendererInstances(found_entity);

        if (found_entity->m_octree != nullptr) {
            DebugLog(
                LogType::Debug,
                "[scene] Remove entity #%u from octree\n",
                found_entity->GetId().value
            );

            found_entity->RemoveFromOctree(GetEngine());
        }

        DebugLog(
            LogType::Debug,
            "Remove entity with ID #%u (with material: %s) from scene with ID #%u\n",
            found_entity->GetId().value,
            found_entity->GetMaterial()
                ? found_entity->GetMaterial()->GetName().Data()
                : " no material ",
            m_id.value
        );

        m_entities.Erase(it);
    }

    m_entities_pending_removal.Clear();
}

void Scene::ForceUpdate()
{
    Update(GetEngine(), 0.01f);
}

void Scene::Update(
    Engine *engine,
    GameCounter::TickUnit delta,
    bool update_octree_visibility
)
{
    AssertReady();

    if (m_entities_pending_addition.Any()) {
        AddPendingEntities();
    }

    if (m_entities_pending_removal.Any()) {
        RemovePendingEntities();
    }

    if (m_camera != nullptr) {
        m_camera->Update(engine, delta);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    EnqueueRenderUpdates();

    if (m_world != nullptr && update_octree_visibility) {
        m_world->GetOctree().CalculateVisibility(this);
    }

    m_environment->Update(engine, delta);

    for (auto &it : m_entities) {
        auto &entity = it.second;
        AssertThrow(entity != nullptr);

        entity->Update(engine, delta);

        if (entity->m_primary_renderer_instance.changed) {
            RequestRendererInstanceUpdate(entity);
        }
    }
}

void Scene::RequestRendererInstanceUpdate(Handle<Entity> &entity)
{
    AssertThrow(entity != nullptr);
    AssertThrow(entity->GetScene() == this);

    if (entity->GetPrimaryRendererInstance() != nullptr) {
        RemoveFromRendererInstance(entity, entity->m_primary_renderer_instance.renderer_instance);
    }
    
    if (auto renderer_instance = GetEngine()->FindOrCreateRendererInstance(entity->GetShader(), entity->GetRenderableAttributes())) {
        renderer_instance->AddEntity(Handle<Entity>(entity));

        entity->m_primary_renderer_instance = {
            .renderer_instance = renderer_instance.Get(),
            .changed = false
        };
    } else {
        DebugLog(
            LogType::Error,
            "Could not find or create optimal RendererInstance for Entity #%lu!\n",
            entity->GetId().value
        );
    }
}

void Scene::RemoveFromRendererInstance(Handle<Entity> &entity, RendererInstance *renderer_instance)
{
    renderer_instance->RemoveEntity(Handle<Entity>(entity));
}

void Scene::RemoveFromRendererInstances(Handle<Entity> &entity)
{
    auto renderer_instances = entity->m_renderer_instances;

    for (auto *renderer_instance : renderer_instances) {
        if (renderer_instance == nullptr) {
            continue;
        }

        // have to inc ref so it can hold it in a temporary container
        renderer_instance->RemoveEntity(Handle<Entity>(entity));
    }
}

void Scene::EnqueueRenderUpdates()
{
    struct {
        BoundingBox aabb;
        Float global_timer;
    } params = {
        .aabb = m_aabb,
        .global_timer = m_environment->GetGlobalTimer()
    };

    GetEngine()->render_scheduler.Enqueue([this, params](...) {
        SceneShaderData shader_data {
            .enabled_render_components_mask = m_environment->GetEnabledRenderComponentsMask(),
            .aabb_max = params.aabb.max.ToVector4(),
            .aabb_min = params.aabb.min.ToVector4(),
            .global_timer = params.global_timer,
            .num_environment_shadow_maps = static_cast<UInt32>(m_environment->HasRenderComponent<ShadowRenderer>()), // callable on render thread only
            .num_lights = static_cast<UInt32>(m_environment->NumLights())
        };

        if (m_camera != nullptr) {
            shader_data.view = m_camera->GetDrawProxy().view;
            shader_data.projection = m_camera->GetDrawProxy().projection;
            shader_data.camera_position = m_camera->GetDrawProxy().position.ToVector4();
            shader_data.camera_near = m_camera->GetDrawProxy().clip_near;
            shader_data.camera_fov = m_camera->GetDrawProxy().fov;
            shader_data.camera_far = m_camera->GetDrawProxy().clip_far;
            shader_data.resolution_x = m_camera->GetDrawProxy().dimensions.width;
            shader_data.resolution_y = m_camera->GetDrawProxy().dimensions.height;
        }

        shader_data.environment_texture_usage = 0u;

        if (GetEngine()->render_state.env_probes.Any()) {//const auto *cubemap_renderer = m_environment->GetRenderComponent<CubemapRenderer>()) {
            // TODO: Make to be packed uvec2 containing indices (each are 1 byte)
            shader_data.environment_texture_index = 0u;//cubemap_renderer->GetComponentIndex();

            for (const auto &it : GetEngine()->render_state.env_probes) {
                if (it.second.Empty()) {
                    continue;
                }

                shader_data.environment_texture_usage |= 1u << it.second.Get();//cubemap_renderer->GetComponentIndex();
            }
        }

        // DebugLog(LogType::Debug, "set %u lights\n", shader_data.num_lights);
        
        GetEngine()->shader_globals->scenes.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

} // namespace hyperion::v2
