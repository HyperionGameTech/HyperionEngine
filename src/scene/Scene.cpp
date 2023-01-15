#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/CubemapRenderer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <math/Halton.hpp>


namespace hyperion::v2 {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(UpdateRenderGroupDrawables) : RenderCommand
{
    RenderGroup *render_group;
    Array<EntityDrawProxy> drawables;

    RENDER_COMMAND(UpdateRenderGroupDrawables)(RenderGroup *render_group, Array<EntityDrawProxy> &&drawables)
        : render_group(render_group),
          drawables(std::move(drawables))
    {
    }

    virtual Result operator()()
    {
        render_group->SetDrawProxies(std::move(drawables));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(BindLights) : RenderCommand
{
    SizeType num_lights;
    Pair<ID<Light>, LightDrawProxy> *lights;

    RENDER_COMMAND(BindLights)(SizeType num_lights, Pair<ID<Light>, LightDrawProxy> *lights)
        : num_lights(num_lights),
          lights(lights)
    {
    }

    virtual Result operator()()
    {
        for (SizeType i = 0; i < num_lights; i++) {
            Engine::Get()->GetRenderState().BindLight(lights[i].first, lights[i].second);
        }

        delete[] lights;

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(BindEnvProbes) : RenderCommand
{
    Array<Pair<ID<EnvProbe>, EnvProbeType>> items;

    RENDER_COMMAND(BindEnvProbes)(Array<Pair<ID<EnvProbe>, EnvProbeType>> &&items)
        : items(std::move(items))
    {
    }

    virtual Result operator()()
    {
        for (const auto &it : items) {
            Engine::Get()->GetRenderState().BindEnvProbe(it.second, it.first);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

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
    m_is_non_world_scene(false),
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

    if (IsWorldScene()) {
        if (!m_tlas) {
            if (Engine::Get()->GetConfig().Get(CONFIG_RT_SUPPORTED) && HasFlags(InitInfo::SCENE_FLAGS_HAS_TLAS)) {
                CreateTLAS();
            } else {
                SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, false);
            }
        }

        InitObject(m_tlas);

        // TODO: Environment should be on World?
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
        auto *lights = new Pair<ID<Light>, LightDrawProxy>[m_lights.Size()];
        SizeType index = 0;

        for (auto &it : m_lights) {
            auto &light = it.second;
            AssertThrow(InitObject(light));

            lights[index++] = { it.first, it.second->GetDrawProxy() };
        }

        RenderCommands::Push<RENDER_COMMAND(BindLights)>(
            index,
            lights
        );
    }

    if (m_env_probes.Any()) {
        // enqueue bind for all in bulk
        Array<Pair<ID<EnvProbe>, EnvProbeType>> items;
            
        ID<EnvProbe> *env_probe_ids = new ID<EnvProbe>[m_env_probes.Size()];
        SizeType index = 0;

        for (auto &it : m_env_probes) {
            if (!it.second->IsAmbientProbe()) {
                items.PushBack({ it.first, it.second->GetEnvProbeType() });
            }
        }

        PUSH_RENDER_COMMAND(
            BindEnvProbes,
            std::move(items)
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

            it.second->SetIsInScene(GetID(), false);

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

void Scene::SetCustomID(IDBase id)
{
    m_custom_id = id;
    m_shader_data_state = ShaderDataState::DIRTY;
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

NodeProxy Scene::AddEntity(Handle<Entity> &&entity)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    if (!entity) {
        return NodeProxy();
    }

    auto node = m_root_node_proxy.AddChild(NodeProxy(new Node));
    node.SetEntity(std::move(entity));

    return node;
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

    entity->SetIsInScene(GetID(), true);

    m_entities_pending_addition.Insert(std::move(entity));

    return true;
}

bool Scene::RemoveEntityInternal(const Handle<Entity> &entity)
{
    // Threads::AssertOnThread(THREAD_GAME);

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

    entity->SetIsInScene(GetID(), false);

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
    Threads::AssertOnThread(THREAD_GAME);

    return m_entities.Find(entity_id) != m_entities.End();
}

void Scene::AddPendingEntities()
{
    if (m_entities_pending_addition.Empty()) {
        return;
    }

    for (auto &entity : m_entities_pending_addition) {
        const ID<Entity> id = entity->GetID();

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
                ? found_entity->GetMaterial()->GetName().LookupString().Data()
                : " no material ",
            GetID().value
        );

        if (m_tlas) {
            if (const auto &blas = found_entity->GetBLAS()) {
                // TODO:
                //m_tlas->RemoveBLAS(blas);
            }
        }

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
    Threads::AssertOnThread(THREAD_GAME);

    if (!light) {
        return false;
    }

    auto insert_result = m_lights.Insert(light->GetID(), std::move(light));
    
    auto it = insert_result.first;
    const bool was_inserted = insert_result.second;

    if (!was_inserted) {
        return false;
    }

    InitObject(it->second);

    return true;
}

bool Scene::AddLight(const Handle<Light> &light)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (!light) {
        return false;
    }

    auto insert_result = m_lights.Insert(light->GetID(), light);
    
    auto it = insert_result.first;
    const bool was_inserted = insert_result.second;

    if (!was_inserted) {
        return false;
    }

    InitObject(it->second);

    return true;
}

bool Scene::RemoveLight(ID<Light> id)
{
    Threads::AssertOnThread(THREAD_GAME);

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
    Threads::AssertOnThread(THREAD_GAME);

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
    Threads::AssertOnThread(THREAD_GAME);

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
    Threads::AssertOnThread(THREAD_GAME);

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

void Scene::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

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

    // update light visibility states
    for (auto &it : m_lights) {
        Handle<Light> &light = it.second;
        
        const bool in_frustum = light->GetType() == LightType::DIRECTIONAL
            || (m_camera.IsValid() && m_camera->GetFrustum().ContainsBoundingSphere(BoundingSphere(light->GetPosition(), light->GetRadius())));

        if (in_frustum != light->IsVisible(GetID())) {
            light->SetIsVisible(GetID(), in_frustum);
        }
    }

    if (IsWorldScene()) {
        // update render environment
        m_environment->Update(delta);

        // update each light
        for (auto &it : m_lights) {
            Handle<Light> &light = it.second;

            light->Update();
        }

        // update each environment probe
        for (auto &it : m_env_probes) {
            Handle<EnvProbe> &env_probe = it.second;

            env_probe->Update();
        }
    }

    m_draw_collection.Reset();

    const RenderableAttributeSet *override_attributes = m_override_renderable_attributes.TryGet();
    
    // update each entity
    for (auto &it : m_entities) {
        Handle<Entity> &entity = it.second;

        entity->Update(delta);

        if (entity->IsRenderable() && IsEntityInFrustum(entity)) {
            PushEntityToRender(entity, override_attributes);
        }
    }

    if (m_parent_scene) {
        for (auto &it : m_parent_scene->GetEntities()) {
            Handle<Entity> &entity = it.second;

            if (entity->IsRenderable() && IsEntityInFrustum(entity)) {
                PushEntityToRender(entity, override_attributes);
            }
        }
    }

    for (auto &it : m_draw_collection) {
        const RenderableAttributeSet &attributes = it.first;
        Array<EntityDrawProxy> &draw_proxies = it.second;

        auto render_group_it = m_render_groups.Find(attributes);

        if (render_group_it == m_render_groups.End() || !render_group_it->second) {
            Handle<RenderGroup> render_group = Engine::Get()->CreateRenderGroup(attributes);

            if (!render_group.IsValid()) {
                DebugLog(LogType::Error, "Render group not valid for attribute set %llu!\n", attributes.GetHashCode().Value());

                continue;
            }

            auto insert_result = m_render_groups.Insert(attributes, std::move(render_group));
            AssertThrow(insert_result.second);

            render_group_it = insert_result.first;
        }
        
        PUSH_RENDER_COMMAND(UpdateRenderGroupDrawables, render_group_it->second.Get(), std::move(draw_proxies));
    }
}

void Scene::PushEntityToRender(const Handle<Entity> &entity, const RenderableAttributeSet *override_attributes)
{
    const Handle<Framebuffer> &framebuffer = m_camera
        ? m_camera->GetFramebuffer()
        : Handle<Framebuffer>::empty;

    RenderableAttributeSet attributes = entity->GetRenderableAttributes();

    if (framebuffer) {
        attributes.framebuffer_id = framebuffer->GetID();
    }

    if (override_attributes) {
        attributes.shader_id = override_attributes->shader_id ? override_attributes->shader_id : attributes.shader_id;
        attributes.material_attributes = override_attributes->material_attributes;
        attributes.stencil_state = override_attributes->stencil_state;
    }

    m_draw_collection.Insert(attributes, entity->GetDrawProxy());
}

bool Scene::IsEntityInFrustum(const Handle<Entity> &entity) const
{
    return entity->GetRenderableAttributes().material_attributes.bucket == BUCKET_UI
        || entity->IsVisibleInScene(GetID());
}

void Scene::Render(Frame *frame, void *push_constant_ptr, SizeType push_constant_size)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrowMsg(m_camera.IsValid(), "Cannot render scene with no Camera attached");
    AssertThrowMsg(m_camera->GetFramebuffer().IsValid(), "Scene has Camera, but no Framebuffer is attached");

    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    m_camera->GetFramebuffer()->BeginCapture(frame_index, command_buffer);

    Engine::Get()->GetRenderState().BindScene(this);

    // TODO: Thread safe container here
    // TODO: If all drawables have been removed, remove the render group?
    for (auto &it : m_render_groups) {
        AssertThrow(it.first.framebuffer_id == m_camera->GetFramebuffer()->GetID());

        if (push_constant_ptr && push_constant_size) {
            it.second->GetPipeline()->SetPushConstants(push_constant_ptr, push_constant_size);
        }

        it.second->Render(frame);
    }

    Engine::Get()->GetRenderState().UnbindScene();

    m_camera->GetFramebuffer()->EndCapture(frame_index, command_buffer);
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
        IDBase custom_id;

        RENDER_COMMAND(UpdateSceneRenderData)(
            ID<Scene> id,
            const BoundingBox &aabb,
            Float global_timer,
            SizeType num_lights,
            const FogParams &fog_params,
            RenderEnvironment *render_environment,
            const CameraDrawProxy &camera_draw_proxy,
            SceneDrawProxy &draw_proxy,
            IDBase custom_id
        ) : id(id),
            aabb(aabb),
            global_timer(global_timer),
            num_lights(num_lights),
            fog_params(fog_params),
            render_environment(render_environment),
            camera_draw_proxy(camera_draw_proxy),
            draw_proxy(draw_proxy),
            custom_id(custom_id)
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

            if (Engine::Get()->GetConfig().Get(CONFIG_TEMPORAL_AA)) {
                // TODO: Is this the main camera in the scene?
                if (draw_proxy.camera.projection[3][3] < MathUtil::epsilon<Float>) {
                    // perspective if [3][3] is zero
                    static const HaltonSequence halton;

                    const UInt halton_index = frame_counter % HaltonSequence::size;

                    jitter = halton.sequence[halton_index];

                    if (frame_counter != 0) {
                        previous_jitter = halton.sequence[(frame_counter - 1) % HaltonSequence::size];
                    }

                    const Vector2 pixel_size = Vector2::one / Vector2(draw_proxy.camera.dimensions);

                    jitter = (jitter * 2.0f - 1.0f) * pixel_size * 0.5f;
                    previous_jitter = (previous_jitter * 2.0f - 1.0f) * pixel_size * 0.5f;

                    offset_matrix[0][3] += jitter.x;
                    offset_matrix[1][3] += jitter.y;
                }
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
            shader_data.custom_index     = custom_id.ToIndex();
            shader_data.enabled_render_components_mask = render_environment->GetEnabledRenderComponentsMask();
            shader_data.taa_params       = Vector4(jitter, previous_jitter);
            shader_data.fog_params       = Vector4(UInt32(fog_params.color), fog_params.start_distance, fog_params.end_distance, 0.0f);

            if (Engine::Get()->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any()) {
                shader_data.environment_texture_index = 0u;

                for (const auto &it : Engine::Get()->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_REFLECTION]) {
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
        m_draw_proxy,
        m_custom_id
    );

    m_shader_data_state = ShaderDataState::CLEAN;
}

bool Scene::CreateTLAS()
{
    AssertThrowMsg(IsWorldScene(), "Can only create TLAS for world scenes");
    AssertIsInitCalled();

    if (m_tlas) {
        // TLAS already exists
        return true;
    }
    
    if (!Engine::Get()->GetConfig().Get(CONFIG_RT_ENABLED)) {
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
