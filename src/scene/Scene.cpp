#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/CubemapRenderer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <math/Halton.hpp>


namespace hyperion::v2 {

Scene::Scene(Handle<Camera> &&camera)
    : Scene(std::move(camera), { })
{
}

Scene::Scene(
    Handle<Camera> &&camera,
    const InitInfo &info
) : EngineComponentBase(info),
    HasDrawProxy(),
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
}
    
void Scene::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    engine->InitObject(m_camera);

    if (!m_tlas) {
        if (engine->GetConfig().Get(CONFIG_RT_SUPPORTED) && HasFlags(InitInfo::SCENE_FLAGS_HAS_TLAS)) {
            CreateTLAS();
        } else {
            SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, false);
        }
    }

    engine->InitObject(m_tlas);

    {
        m_environment->Init(engine);

        if (m_tlas) {
            m_environment->SetTLAS(m_tlas);
        }
    }

    for (auto &it : m_entities) {
        auto &entity = it.second;
        AssertThrow(engine->InitObject(entity));
    }

    if (m_entities_pending_removal.Any()) {
        RemovePendingEntities();
    }

    if (m_entities_pending_addition.Any()) {
        for (auto &entity : m_entities_pending_addition) {
            AssertThrow(entity);
            
            engine->InitObject(entity);
        }

        AddPendingEntities();
    }

    if (m_lights.Any()) {
        // enqueue bind for all in bulk
        Light::ID *light_ids = new Light::ID[m_lights.Size()];
        SizeType index = 0;

        for (auto &it : m_lights) {
            auto &light = it.second;
            AssertThrow(engine->InitObject(light));

            light_ids[index++] = it.first;
        }

        engine->GetRenderScheduler().Enqueue([engine, light_ids, num_lights = index](...) mutable {
            for (SizeType i = 0; i < num_lights; i++) {
                engine->GetRenderState().BindLight(light_ids[i]);
            }

            delete[] light_ids;

            HYPERION_RETURN_OK;
        });
    }

    if (m_env_probes.Any()) {
        // enqueue bind for all in bulk
        EnvProbe::ID *env_probe_ids = new EnvProbe::ID[m_env_probes.Size()];
        SizeType index = 0;

        for (auto &it : m_env_probes) {
            auto &env_probe = it.second;
            AssertThrow(engine->InitObject(env_probe));

            env_probe_ids[index++] = it.first;
        }

        engine->GetRenderScheduler().Enqueue([engine, env_probe_ids, num_env_probes = index](...) mutable {
            for (SizeType i = 0; i < num_env_probes; i++) {
                engine->GetRenderState().BindEnvProbe(env_probe_ids[i]);
            }

            delete[] env_probe_ids;

            HYPERION_RETURN_OK;
        });
    }

    SetReady(true);

    OnTeardown([this]() {
        auto *engine = GetEngine();

        m_camera.Reset();
        m_tlas.Reset();
        m_environment.Reset();

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
}

void Scene::SetCamera(Handle<Camera> &&camera)
{
    m_camera = std::move(camera);

    if (IsInitCalled()) {
        GetEngine()->InitObject(m_camera);
    }
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
                entity->GetID().value
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
            entity->GetID().value
        );
#endif

        entity->AddToOctree(GetEngine(), m_world->GetOctree());
    }
}

bool Scene::AddEntity(Handle<Entity> &&entity)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    if (!entity) {
        return false;
    }

    auto node = m_root_node_proxy.AddChild(NodeProxy(
        new Node(entity->GetName(), entity->GetTransform())
    ));

    node.SetEntity(std::move(entity));

    return true;
}

bool Scene::RemoveEntity(const Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    if (!entity) {
        return false;
    }

    // breadth-first search
    Queue<NodeProxy *> queue;
    queue.Push(&m_root_node_proxy);

    while (queue.Any()) {
        NodeProxy *parent = queue.Pop();

        for (auto &child : parent->GetChildren()) {
            if (!child) {
                continue;
            }

            if (child.GetEntity() == entity) {
                parent->Get()->RemoveChild(&child);

                return true;
            }

            queue.Push(&child);
        }
    }

    return false;
}

bool Scene::AddEntityInternal(Handle<Entity> &&entity)
{
    // Threads::AssertOnThread(THREAD_GAME);
    // AssertReady();

    if (!entity) {
        return false;
    }

    if (m_entities.Contains(entity->GetID())) {
        DebugLog(
            LogType::Warn,
            "Entity #%u already exists in Scene #%u\n",
            entity->GetID().value,
            m_id.value
        );

        return false;
    }

    if (IsInitCalled()) {
        GetEngine()->InitObject(entity);
    }

    entity->SetScene(this);

    m_entities_pending_addition.Insert(std::move(entity));

    return true;
}

bool Scene::RemoveEntityInternal(const Handle<Entity> &entity)
{
    // Threads::AssertOnThread(THREAD_GAME);
    // AssertReady();

    if (!entity) {
        return false;
    }

    const auto it = m_entities.Find(entity->GetID());

    if (it == m_entities.End()) {
        DebugLog(
            LogType::Warn,
            "Could not remove entity with id #%u: not found\n",
            entity->GetID().value
        );

        return false;
    }

    entity->SetScene(nullptr);

    if (m_entities_pending_removal.Contains(entity->GetID())) {
        DebugLog(
            LogType::Warn,
            "Could not remove entity with id #%u: already pending removal\n",
            entity->GetID().value
        );

        return false;
    }

    m_entities_pending_removal.Insert(entity->GetID());

    return true;
}

bool Scene::HasEntity(Entity::ID id) const
{
    // Threads::AssertOnThread(THREAD_GAME);

    return m_entities.Find(id) != m_entities.End();
}

void Scene::AddPendingEntities()
{
    if (m_entities_pending_addition.Empty()) {
        return;
    }

    for (auto &entity : m_entities_pending_addition) {
        const auto id = entity->GetID();

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
                    entity->GetID().value
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
                    entity->GetID().value
                );
#endif

                entity->AddToOctree(GetEngine(), m_world->GetOctree());
            }
        }

        if (m_tlas) {
            if (const auto &blas = entity->GetBLAS()) {
                m_tlas->AddBLAS(Handle<BLAS>(blas));       
            }
        }

        m_environment->OnEntityAdded(entity);
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
                found_entity->GetID().value
            );

            found_entity->RemoveFromOctree(GetEngine());
        }

        DebugLog(
            LogType::Debug,
            "Remove entity with ID #%u (with material: %s) from scene with ID #%u\n",
            found_entity->GetID().value,
            found_entity->GetMaterial()
                ? found_entity->GetMaterial()->GetName().Data()
                : " no material ",
            m_id.value
        );

        if (m_tlas) {
            if (const auto &blas = found_entity->GetBLAS()) {
                //m_tlas->RemoveBLAS(blas);
            }
        }

        m_environment->OnEntityRemoved(it->second);
        m_entities.Erase(it);
    }

    m_entities_pending_removal.Clear();
}

const Handle<Entity> &Scene::FindEntityWithID(const Entity::ID &id) const
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_root_node_proxy);
    return m_root_node_proxy.Get()->FindEntityWithID(id);
}

bool Scene::AddLight(Handle<Light> &&light)
{
    if (!light) {
        return false;
    }

    auto insert_result = m_lights.Insert(light->GetID(), std::move(light));
    
    auto it = insert_result.first;
    const bool was_inserted = insert_result.second;

    if (!was_inserted) {
        return false;
    }

    if (IsInitCalled()) {
        if (GetEngine()->InitObject(it->second)) {
            it->second->EnqueueBind(GetEngine());
        }
    }

    return true;
}

bool Scene::AddLight(const Handle<Light> &light)
{
    if (!light) {
        return false;
    }

    auto insert_result = m_lights.Insert(light->GetID(), light);
    
    auto it = insert_result.first;
    const bool was_inserted = insert_result.second;

    if (!was_inserted) {
        return false;
    }

    if (IsInitCalled()) {
        if (GetEngine()->InitObject(it->second)) {
            it->second->EnqueueBind(GetEngine());
        }
    }

    return true;
}

bool Scene::RemoveLight(Light::ID id)
{
    auto it = m_lights.Find(id);

    if (it == m_lights.End()) {
        return false;
    }

    if (IsInitCalled()) {
        if (it->second) {
            it->second->EnqueueUnbind(GetEngine());
        }
    }

    return m_lights.Erase(it);
}

bool Scene::AddEnvProbe(Handle<EnvProbe> &&env_probe)
{
    if (!env_probe) {
        return false;
    }

    auto insert_result = m_env_probes.Insert(env_probe->GetID(), std::move(env_probe));
    
    auto it = insert_result.first;
    const bool was_inserted = insert_result.second;

    if (!was_inserted) {
        return false;
    }

    if (IsInitCalled()) {
        if (GetEngine()->InitObject(it->second)) {
            it->second->EnqueueBind(GetEngine());
        }
    }

    return true;
}

bool Scene::AddEnvProbe(const Handle<EnvProbe> &env_probe)
{
    if (!env_probe) {
        return false;
    }

    auto insert_result = m_env_probes.Insert(env_probe->GetID(), env_probe);
    
    auto it = insert_result.first;
    const bool was_inserted = insert_result.second;

    if (!was_inserted) {
        return false;
    }

    if (IsInitCalled()) {
        if (GetEngine()->InitObject(it->second)) {
            it->second->EnqueueBind(GetEngine());
        }
    }

    return true;
}

bool Scene::RemoveEnvProbe(EnvProbe::ID id)
{
    auto it = m_env_probes.Find(id);

    if (it == m_env_probes.End()) {
        return false;
    }

    if (IsInitCalled()) {
        if (it->second) {
            it->second->EnqueueUnbind(GetEngine());
        }
    }

    return m_env_probes.Erase(it);
}

void Scene::ForceUpdate()
{
    AssertReady();

    Update(GetEngine(), 0.01f);
}

void Scene::Update(
    Engine *engine,
    GameCounter::TickUnit delta,
    bool update_octree_visibility
)
{
    AssertReady();

    if (m_entities_pending_removal.Any()) {
        RemovePendingEntities();
    }

    if (m_entities_pending_addition.Any()) {
        AddPendingEntities();
    }

    if (m_camera) {
        m_camera->Update(engine, delta);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    EnqueueRenderUpdates();

    if (!IsVirtualScene()) {
        // update render environment
        m_environment->Update(engine, delta);

        // update each light
        for (auto &it : m_lights) {
            auto &light = it.second;

            light->Update(engine);
        }

        // update each environment probe
        for (auto &it : m_env_probes) {
            auto &env_probe = it.second;

            env_probe->Update(engine);
        }

        // update each entity
        for (auto &it : m_entities) {
            auto &entity = it.second;

            entity->Update(engine, delta);

            if (entity->m_primary_renderer_instance.changed) {
                RequestRendererInstanceUpdate(entity);
            }
        }
    }
}

void Scene::RequestRendererInstanceUpdate(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(entity != nullptr);
    AssertThrow(entity->GetScene() == this);

    if (entity->GetPrimaryRendererInstance() != nullptr) {
        RemoveFromRendererInstance(entity, entity->m_primary_renderer_instance.renderer_instance);
    }

    if (entity->IsRenderable()) {
        if (auto renderer_instance = GetEngine()->FindOrCreateRendererInstance(entity->GetShader(), entity->GetRenderableAttributes())) {
            renderer_instance->AddEntity(Handle<Entity>(entity));

            entity->m_primary_renderer_instance.renderer_instance = renderer_instance.Get();
        } else {
            DebugLog(
                LogType::Error,
                "Could not find or create optimal RendererInstance for Entity #%lu!\n",
                entity->GetID().value
            );
        }
    }

    // don't continue requesting, even if we couldn't find or create a RendererInstance
    entity->m_primary_renderer_instance.changed = false;
}

void Scene::RemoveFromRendererInstance(Handle<Entity> &entity, RendererInstance *renderer_instance)
{
    renderer_instance->RemoveEntity(Handle<Entity>(entity));
    entity->m_primary_renderer_instance.renderer_instance = nullptr;
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
        UInt32 frame_counter;
        UInt32 num_lights;
        FogParams fog_params;
    } params = {
        .aabb = m_root_node_proxy.GetWorldAABB(),
        .global_timer = m_environment->GetGlobalTimer(),
        .frame_counter = m_environment->GetFrameCounter(),
        .num_lights = static_cast<UInt32>(m_lights.Size()),
        .fog_params = m_fog_params
    };

    GetEngine()->render_scheduler.Enqueue([this, params](...) {
        m_draw_proxy.frame_counter = params.frame_counter;

        if (m_camera) {
            m_draw_proxy.camera = m_camera->GetDrawProxy();
        } else {
            m_draw_proxy.camera = { };
        }

        Matrix4 offset_matrix;
        Vector2 jitter;
        Vector2 previous_jitter;

        // TODO: Is this the main camera in the scene?
        if (m_draw_proxy.camera.projection[3][3] < MathUtil::epsilon<Float>) {
            // perspective if [3][3] is zero
            static const HaltonSequence halton;

            const UInt halton_index = params.frame_counter % HaltonSequence::size;

            jitter = halton.sequence[halton_index];

            if (params.frame_counter != 0) {
                previous_jitter = halton.sequence[(params.frame_counter - 1) % HaltonSequence::size];
            }

            offset_matrix = Matrix4::Jitter(
                m_draw_proxy.camera.dimensions.width,
                m_draw_proxy.camera.dimensions.height,
                jitter
            );
        }
        
        SceneShaderData shader_data { };
        shader_data.view = m_draw_proxy.camera.view;
        shader_data.projection = offset_matrix * m_draw_proxy.camera.projection;
        shader_data.previous_view = m_draw_proxy.camera.previous_view;
        shader_data.camera_position = m_draw_proxy.camera.position.ToVector4();
        shader_data.camera_direction = m_draw_proxy.camera.direction.ToVector4();
        shader_data.camera_up = m_draw_proxy.camera.up.ToVector4();
        shader_data.camera_near = m_draw_proxy.camera.clip_near;
        shader_data.camera_fov = m_draw_proxy.camera.fov;
        shader_data.camera_far = m_draw_proxy.camera.clip_far;
        shader_data.resolution_x = m_draw_proxy.camera.dimensions.width;
        shader_data.resolution_y = m_draw_proxy.camera.dimensions.height;
        shader_data.environment_texture_usage = 0u;
        shader_data.aabb_max = params.aabb.max.ToVector4();
        shader_data.aabb_min = params.aabb.min.ToVector4();
        shader_data.global_timer = params.global_timer;
        shader_data.frame_counter = params.frame_counter;
        shader_data.num_lights = params.num_lights;
        shader_data.enabled_render_components_mask = m_environment->GetEnabledRenderComponentsMask();
        shader_data.taa_params = Vector4(jitter, previous_jitter);
        shader_data.fog_params = Vector4(UInt32(params.fog_params.color), params.fog_params.start_distance, params.fog_params.end_distance, 0.0f);

        if (GetEngine()->render_state.env_probes.Any()) {
            // TODO: Make to be packed uvec2 containing indices (each are 1 byte)
            shader_data.environment_texture_index = 0u;

            for (const auto &it : GetEngine()->render_state.env_probes) {
                if (it.second.Empty()) {
                    continue;
                }

                shader_data.environment_texture_usage |= 1u << it.second.Get();
            }
        }
        
        GetEngine()->GetRenderData()->scenes.Set(m_id.ToIndex(), shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

bool Scene::CreateTLAS()
{
    AssertIsInitCalled();

    if (m_tlas) {
        // TLAS already exists
        return true;
    }
    
    if (!GetEngine()->GetDevice()->GetFeatures().IsRaytracingSupported()) {
        // cannot create TLAS if RT is not supported.
        SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, false);

        return false;
    }

    m_tlas = GetEngine()->CreateHandle<TLAS>();

    if (IsReady()) {
        GetEngine()->InitObject(m_tlas);

        m_environment->SetTLAS(m_tlas);
    }

    SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, true);

    return true;
}


} // namespace hyperion::v2
