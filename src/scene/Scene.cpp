#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/CubemapRenderer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <math/Halton.hpp>


namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(BindLights) : RenderCommand
{
    SizeType num_lights;
    ID<Light> *ids;

    RENDER_COMMAND(BindLights)(SizeType num_lights, ID<Light> *ids)
        : num_lights(num_lights),
          ids(ids)
    {
    }

    virtual Result operator()()
    {
        for (SizeType i = 0; i < num_lights; i++) {
            Engine::Get()->GetRenderState().BindLight(ids[i]);
        }

        delete[] ids;

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(BindEnvProbes) : RenderCommand
{
    SizeType num_env_probes;
    ID<EnvProbe> *ids;

    RENDER_COMMAND(BindEnvProbes)(SizeType num_env_probes, ID<EnvProbe> *ids)
        : num_env_probes(num_env_probes),
          ids(ids)
    {
    }

    virtual Result operator()()
    {
        for (SizeType i = 0; i < num_env_probes; i++) {
            Engine::Get()->GetRenderState().BindEnvProbe(ids[i]);
        }

        delete[] ids;

        HYPERION_RETURN_OK;
    }
};

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
    
void Scene::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init();

    InitObject(m_camera);

    if (!m_tlas) {
        if (Engine::Get()->GetConfig().Get(CONFIG_RT_SUPPORTED) && HasFlags(InitInfo::SCENE_FLAGS_HAS_TLAS)) {
            CreateTLAS();
        } else {
            SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, false);
        }
    }

    InitObject(m_tlas);

    {
        m_environment->Init();

        if (m_tlas) {
            m_environment->SetTLAS(m_tlas);
        }
    }

    for (auto &it : m_entities) {
        auto &entity = it.second;
        AssertThrow(InitObject(entity));
    }

    if (m_entities_pending_removal.Any()) {
        RemovePendingEntities();
    }

    if (m_entities_pending_addition.Any()) {
        for (auto &entity : m_entities_pending_addition) {
            AssertThrow(entity);
            
            InitObject(entity);
        }

        AddPendingEntities();
    }

    if (m_lights.Any()) {
        // enqueue bind for all in bulk
        ID<Light> *light_ids = new ID<Light>[m_lights.Size()];
        SizeType index = 0;

        for (auto &it : m_lights) {
            auto &light = it.second;
            AssertThrow(InitObject(light));

            light_ids[index++] = it.first;
        }

        RenderCommands::Push<RENDER_COMMAND(BindLights)>(
            index,
            light_ids
        );
    }

    if (m_env_probes.Any()) {
        // enqueue bind for all in bulk
        ID<EnvProbe> *env_probe_ids = new ID<EnvProbe>[m_env_probes.Size()];
        SizeType index = 0;

        for (auto &it : m_env_probes) {
            auto &env_probe = it.second;
            AssertThrow(InitObject(env_probe));

            env_probe_ids[index++] = it.first;
        }

        RenderCommands::Push<RENDER_COMMAND(BindEnvProbes)>(
            index,
            env_probe_ids
        );
    }

    SetReady(true);

    OnTeardown([this]() {
        auto *engine = Engine::Get();

        m_camera.Reset();
        m_tlas.Reset();
        m_environment.Reset();

        m_root_node_proxy.Get()->SetScene(nullptr);

        for (auto &it : m_entities) {
            AssertThrow(it.second != nullptr);

            it.second->SetScene(nullptr);

            RemoveFromRenderGroups(it.second);

            if (it.second->m_octree != nullptr) {
                it.second->RemoveFromOctree();
            }
        }

        m_entities.Clear();
        m_entities_pending_addition.Clear();
        m_entities_pending_removal.Clear();

        HYP_SYNC_RENDER();

        SetReady(false);
    });
}

void Scene::SetCamera(Handle<Camera> &&camera)
{
    m_camera = std::move(camera);

    InitObject(m_camera);
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

            entity->RemoveFromOctree();
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

        entity->AddToOctree(m_world->GetOctree());
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

bool Scene::RemoveEntity(ID<Entity> entity_id)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    if (!entity_id) {
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

            if (child.GetEntity().GetID() == entity_id) {
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

    InitObject(entity);

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

bool Scene::HasEntity(ID<Entity> entity_id) const
{
    // Threads::AssertOnThread(THREAD_GAME);

    return m_entities.Find(entity_id) != m_entities.End();
}

void Scene::AddPendingEntities()
{
    if (m_entities_pending_addition.Empty()) {
        return;
    }

    for (auto &entity : m_entities_pending_addition) {
        const ID<Entity> id = entity->GetID();

        if (entity->IsRenderable() && !entity->GetPrimaryRenderGroup()) {
            if (auto renderer_instance = Engine::Get()->FindOrCreateRenderGroup(entity->GetShader(), entity->GetRenderableAttributes())) {
                renderer_instance->AddEntity(Handle<Entity>(entity));
                entity->EnqueueRenderUpdates();

                entity->m_primary_renderer_instance = {
                    .renderer_instance = renderer_instance.Get(),
                    .changed = false
                };
            } else {
                DebugLog(
                    LogType::Error,
                    "Could not find or create optimal RenderGroup for Entity #%lu!\n",
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

                entity->AddToOctree(m_world->GetOctree());
            }
        }

        if (m_tlas) {
            if (const auto &blas = entity->GetBLAS()) {
                m_tlas->AddBLAS(Handle<BLAS>(blas));       
            }
        }

        m_environment->OnEntityAdded(entity);
        m_entities.Insert(id, std::move(entity));
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
        AssertThrow(found_entity.IsValid());

        RemoveFromRenderGroups(found_entity);

        if (found_entity->m_octree != nullptr) {
            DebugLog(
                LogType::Debug,
                "[scene] Remove entity #%u from octree\n",
                found_entity->GetID().value
            );

            found_entity->RemoveFromOctree();
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
                // TODO:
                //m_tlas->RemoveBLAS(blas);
            }
        }

        m_environment->OnEntityRemoved(it->second);
        m_entities.Erase(it);
    }

    m_entities_pending_removal.Clear();
}

const Handle<Entity> &Scene::FindEntityWithID(ID<Entity> entity_id) const
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_root_node_proxy);
    return m_root_node_proxy.Get()->FindEntityWithID(entity_id);
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

    if (InitObject(it->second)) {
        it->second->EnqueueBind();
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

    if (InitObject(it->second)) {
        it->second->EnqueueBind();
    }

    return true;
}

bool Scene::RemoveLight(ID<Light> id)
{
    auto it = m_lights.Find(id);

    if (it == m_lights.End()) {
        return false;
    }

    if (it->second) {
        it->second->EnqueueUnbind();
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

    if (InitObject(it->second)) {
        it->second->EnqueueBind();
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

    if (InitObject(it->second)) {
        it->second->EnqueueBind();
    }

    return true;
}

bool Scene::RemoveEnvProbe(ID<EnvProbe> id)
{
    auto it = m_env_probes.Find(id);

    if (it == m_env_probes.End()) {
        return false;
    }

    if (it->second) {
        it->second->EnqueueUnbind();
    }

    return m_env_probes.Erase(it);
}

void Scene::ForceUpdate()
{
    AssertReady();

    Update(0.0166f);
}

void Scene::Update(
    
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
        m_camera->Update(delta);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    EnqueueRenderUpdates();

    if (!IsVirtualScene()) {
        // update render environment
        m_environment->Update(delta);

        // update each light
        for (auto &it : m_lights) {
            auto &light = it.second;

            light->Update();
        }

        // update each environment probe
        for (auto &it : m_env_probes) {
            auto &env_probe = it.second;

            env_probe->Update();
        }

        // update each entity
        for (auto &it : m_entities) {
            auto &entity = it.second;

            entity->Update(delta);

            if (entity->m_primary_renderer_instance.changed) {
                RequestRenderGroupUpdate(entity);
            }
        }
    }
}

void Scene::RequestRenderGroupUpdate(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(entity != nullptr);
    AssertThrow(entity->GetScene() == this);

    if (entity->GetPrimaryRenderGroup() != nullptr) {
        RemoveFromRenderGroup(entity, entity->m_primary_renderer_instance.renderer_instance);
    }

    if (entity->IsRenderable()) {
        if (auto renderer_instance = Engine::Get()->FindOrCreateRenderGroup(entity->GetShader(), entity->GetRenderableAttributes())) {
            renderer_instance->AddEntity(Handle<Entity>(entity));

            entity->m_primary_renderer_instance.renderer_instance = renderer_instance.Get();
        } else {
            DebugLog(
                LogType::Error,
                "Could not find or create optimal RenderGroup for Entity #%lu!\n",
                entity->GetID().value
            );
        }
    }

    // don't continue requesting, even if we couldn't find or create a RenderGroup
    entity->m_primary_renderer_instance.changed = false;
}

void Scene::RemoveFromRenderGroup(Handle<Entity> &entity, RenderGroup *renderer_instance)
{
    renderer_instance->RemoveEntity(Handle<Entity>(entity));
    entity->m_primary_renderer_instance.renderer_instance = nullptr;
}

void Scene::RemoveFromRenderGroups(Handle<Entity> &entity)
{
    auto renderer_instances = entity->m_render_groups;

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
    struct RENDER_COMMAND(UpdateSceneRenderData) : RenderCommand
    {
        ID<Scene> id;
        BoundingBox aabb;
        Float global_timer;
        SizeType num_lights;
        FogParams fog_params;
        RenderEnvironment *render_environment;
        CameraDrawProxy camera_draw_proxy;
        SceneDrawProxy &draw_proxy;

        RENDER_COMMAND(UpdateSceneRenderData)(
            ID<Scene> id,
            const BoundingBox &aabb,
            Float global_timer,
            SizeType num_lights,
            const FogParams &fog_params,
            RenderEnvironment *render_environment,
            const CameraDrawProxy &camera_draw_proxy,
            SceneDrawProxy &draw_proxy
        ) : id(id),
            aabb(aabb),
            global_timer(global_timer),
            num_lights(num_lights),
            fog_params(fog_params),
            render_environment(render_environment),
            camera_draw_proxy(camera_draw_proxy),
            draw_proxy(draw_proxy)
        {
        }

        virtual Result operator()()
        {
            const UInt frame_counter = render_environment->GetFrameCounter();

            draw_proxy.frame_counter = frame_counter;
            draw_proxy.camera = camera_draw_proxy;

            Matrix4 offset_matrix;
            Vector2 jitter;
            Vector2 previous_jitter;

            // TODO: Is this the main camera in the scene?
            if (draw_proxy.camera.projection[3][3] < MathUtil::epsilon<Float>) {
                // perspective if [3][3] is zero
                static const HaltonSequence halton;

                const UInt halton_index = frame_counter % HaltonSequence::size;

                jitter = halton.sequence[halton_index];

                if (frame_counter != 0) {
                    previous_jitter = halton.sequence[(frame_counter - 1) % HaltonSequence::size];
                }

                offset_matrix = Matrix4::Jitter(
                    draw_proxy.camera.dimensions.width,
                    draw_proxy.camera.dimensions.height,
                    jitter
                );
            }
            
            SceneShaderData shader_data { };
            shader_data.view             = draw_proxy.camera.view;
            shader_data.projection       = offset_matrix * draw_proxy.camera.projection;
            shader_data.previous_view    = draw_proxy.camera.previous_view;
            shader_data.camera_position  = draw_proxy.camera.position.ToVector4();
            shader_data.camera_direction = draw_proxy.camera.direction.ToVector4();
            shader_data.camera_up        = draw_proxy.camera.up.ToVector4();
            shader_data.camera_near      = draw_proxy.camera.clip_near;
            shader_data.camera_fov       = draw_proxy.camera.fov;
            shader_data.camera_far       = draw_proxy.camera.clip_far;
            shader_data.resolution_x     = draw_proxy.camera.dimensions.width;
            shader_data.resolution_y     = draw_proxy.camera.dimensions.height;
            shader_data.environment_texture_usage = 0u;
            shader_data.aabb_max         = aabb.max.ToVector4();
            shader_data.aabb_min         = aabb.min.ToVector4();
            shader_data.global_timer     = global_timer;
            shader_data.frame_counter    = frame_counter;
            shader_data.num_lights       = UInt32(num_lights);
            shader_data.enabled_render_components_mask = render_environment->GetEnabledRenderComponentsMask();
            shader_data.taa_params       = Vector4(jitter, previous_jitter);
            shader_data.fog_params       = Vector4(UInt32(fog_params.color), fog_params.start_distance, fog_params.end_distance, 0.0f);

            if (Engine::Get()->render_state.env_probes.Any()) {
                // TODO: Make to be packed uvec2 containing indices (each are 1 byte)
                shader_data.environment_texture_index = 0u;

                for (const auto &it : Engine::Get()->render_state.env_probes) {
                    if (it.second.Empty()) {
                        continue;
                    }

                    shader_data.environment_texture_usage |= 1u << it.second.Get();
                }
            }
            
            Engine::Get()->GetRenderData()->scenes.Set(id.ToIndex(), shader_data);

            HYPERION_RETURN_OK;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(UpdateSceneRenderData)>(
        m_id,
        m_root_node_proxy.GetWorldAABB(),
        m_environment->GetGlobalTimer(),
        m_lights.Size(),
        m_fog_params,
        m_environment.Get(),
        m_camera ? m_camera->GetDrawProxy() : CameraDrawProxy { },
        m_draw_proxy
    );

    m_shader_data_state = ShaderDataState::CLEAN;
}

bool Scene::CreateTLAS()
{
    AssertIsInitCalled();

    if (m_tlas) {
        // TLAS already exists
        return true;
    }
    
    if (!Engine::Get()->GetGPUDevice()->GetFeatures().IsRaytracingSupported()) {
        // cannot create TLAS if RT is not supported.
        SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, false);

        return false;
    }

    m_tlas = CreateObject<TLAS>();

    if (IsReady()) {
        InitObject(m_tlas);

        m_environment->SetTLAS(m_tlas);
    }

    SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, true);

    return true;
}


} // namespace hyperion::v2
