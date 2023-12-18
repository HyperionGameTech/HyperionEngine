#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/CubemapRenderer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <math/Halton.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>


namespace hyperion::v2 {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(BindLights) : renderer::RenderCommand
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
            g_engine->GetRenderState().BindLight(lights[i].first, lights[i].second);
        }

        delete[] lights;

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(BindEnvProbes) : renderer::RenderCommand
{
    Array<Pair<ID<EnvProbe>, EnvProbeType>> items;

    RENDER_COMMAND(BindEnvProbes)(Array<Pair<ID<EnvProbe>, EnvProbeType>> &&items)
        : items(std::move(items))
    {
    }

    virtual Result operator()()
    {
        for (const auto &it : items) {
            g_engine->GetRenderState().BindEnvProbe(it.second, it.first);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

Scene::Scene()
    : Scene(Handle<Camera>(), { })
{
}

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
    m_render_list.SetCamera(m_camera);

    if (IsWorldScene()) {
        if (!m_tlas) {
            if (g_engine->GetConfig().Get(CONFIG_RT_SUPPORTED) && HasFlags(InitInfo::SCENE_FLAGS_HAS_TLAS)) {
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

        PUSH_RENDER_COMMAND(BindLights, 
            index,
            lights
        );
    }

    if (m_env_probes.Any()) {
        // enqueue bind for all in bulk
        Array<Pair<ID<EnvProbe>, EnvProbeType>> items;
        items.Reserve(m_env_probes.Size());

        for (auto &it : m_env_probes) {
            if (!it.second->IsControlledByEnvGrid()) {
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
        auto *engine = g_engine;

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

    m_render_list.SetCamera(m_camera);
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

const Handle<Entity> &Scene::FindEntityByName(Name name) const
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_root_node_proxy);
    return m_root_node_proxy.Get()->FindEntityByName(name);
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

    return m_lights.Erase(it) != m_lights.End();
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

    const auto insert_result = m_env_probes.Insert(env_probe->GetID(), env_probe);
    
    const auto it = insert_result.first;
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

    const auto it = m_env_probes.Find(id);

    if (it == m_env_probes.End()) {
        return false;
    }

    if (it->second) {
        it->second->EnqueueUnbind();
    }

    return m_env_probes.Erase(it) != m_env_probes.End();
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

    ID<Camera> camera_id;

    if (m_camera) {
        camera_id = m_camera->GetID();

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
        
        const bool is_light_in_frustum = light->GetType() == LightType::DIRECTIONAL
            || (m_camera.IsValid() && m_camera->GetFrustum().ContainsBoundingSphere(BoundingSphere(light->GetPosition(), light->GetRadius())));

        light->SetIsVisible(camera_id, is_light_in_frustum);
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

            env_probe->Update(delta);
        }
    }

    // TEMP
    if (m_camera) {
        const Vector3 &camera_position = m_camera->GetTranslation();

        std::sort(m_entities.Begin(), m_entities.End(), [&camera_position](const auto &lhs, const auto &rhs) {
            return camera_position.Distance(lhs.second->GetTranslation()) > camera_position.Distance(rhs.second->GetTranslation());
        });
    }

    // update each entity
    for (auto &it : m_entities) {
        Handle<Entity> &entity = it.second;

        entity->Update(delta);
    }
}

void Scene::CollectEntities(
    RenderList &render_list,
    const Handle<Camera> &camera,
    const Bitset &bucket_bits,
    Optional<RenderableAttributeSet> override_attributes,
    bool skip_frustum_culling
) const
{
    Threads::AssertOnThread(THREAD_GAME);

    // clear out existing entities before populating
    render_list.ClearEntities();

    if (!camera) {
        return;
    }

    const ID<Camera> camera_id = camera->GetID();

    RenderableAttributeSet *override_attributes_ptr = override_attributes.TryGet();
    const UInt32 override_flags = override_attributes_ptr ? override_attributes_ptr->GetOverrideFlags() : 0;
    
    const UInt8 visibility_cursor = g_engine->GetWorld()->GetOctree().LoadVisibilityCursor();

    // push all entities to render if they are visible to the given camera
    for (auto &it : m_entities) {
        const Handle<Entity> &entity = it.second;

        if (
            entity->IsRenderable()
            && bucket_bits.Test(entity->GetRenderableAttributes().GetMaterialAttributes().bucket)
            && (skip_frustum_culling || IsEntityInFrustum(entity, camera_id, visibility_cursor))
        ) {
            render_list.PushEntityToRender(camera, entity, override_attributes_ptr);
        }
    }
}

void Scene::CollectEntities(
    RenderList &render_list,
    const Handle<Camera> &camera,
    Optional<RenderableAttributeSet> override_attributes,
    bool skip_frustum_culling
) const
{
    Threads::AssertOnThread(THREAD_GAME);

    // clear out existing entities before populating
    render_list.ClearEntities();

    if (!camera) {
        return;
    }

    const ID<Camera> camera_id = camera->GetID();

    RenderableAttributeSet *override_attributes_ptr = override_attributes.TryGet();
    const UInt32 override_flags = override_attributes_ptr ? override_attributes_ptr->GetOverrideFlags() : 0;
    
    const UInt8 visibility_cursor = g_engine->GetWorld()->GetOctree().LoadVisibilityCursor();
    
    // push all entities to render if they are visible to the given camera
    for (auto &it : m_entities) {
        const Handle<Entity> &entity = it.second;

        if (entity->IsRenderable() && (skip_frustum_culling || IsEntityInFrustum(entity, camera_id, visibility_cursor))) {
            render_list.PushEntityToRender(camera, entity, override_attributes_ptr);
        }
    }
}

bool Scene::IsEntityInFrustum(const Handle<Entity> &entity, ID<Camera> camera_id, UInt8 visibility_cursor) const
{
    if (!camera_id) {
        return false;
    }

    return entity->GetRenderableAttributes().GetMaterialAttributes().bucket == BUCKET_UI
        || entity->IsVisibleToCamera(camera_id, visibility_cursor);
}

void Scene::EnqueueRenderUpdates()
{
    struct RENDER_COMMAND(UpdateSceneRenderData) : renderer::RenderCommand
    {
        ID<Scene> id;
        BoundingBox aabb;
        Float global_timer;
        SizeType num_lights;
        FogParams fog_params;
        RenderEnvironment *render_environment;
        SceneDrawProxy &draw_proxy;

        RENDER_COMMAND(UpdateSceneRenderData)(
            ID<Scene> id,
            const BoundingBox &aabb,
            Float global_timer,
            SizeType num_lights,
            const FogParams &fog_params,
            RenderEnvironment *render_environment,
            SceneDrawProxy &draw_proxy
        ) : id(id),
            aabb(aabb),
            global_timer(global_timer),
            num_lights(num_lights),
            fog_params(fog_params),
            render_environment(render_environment),
            draw_proxy(draw_proxy)
        {
        }

        virtual Result operator()()
        {
            const UInt frame_counter = render_environment->GetFrameCounter();

            draw_proxy.frame_counter = frame_counter;

            SceneShaderData shader_data { };
            shader_data.aabb_max         = Vector4(aabb.max, 1.0f);
            shader_data.aabb_min         = Vector4(aabb.min, 1.0f);
            shader_data.fog_params       = Vector4(Float(fog_params.color.Packed()), fog_params.start_distance, fog_params.end_distance, 0.0f);
            shader_data.global_timer     = global_timer;
            shader_data.frame_counter    = frame_counter;
            shader_data.enabled_render_components_mask = render_environment->GetEnabledRenderComponentsMask();
            
            g_engine->GetRenderData()->scenes.Set(id.ToIndex(), shader_data);

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(UpdateSceneRenderData, 
        m_id,
        m_root_node_proxy.GetWorldAABB(),
        m_environment->GetGlobalTimer(),
        m_lights.Size(),
        m_fog_params,
        m_environment.Get(),
        m_draw_proxy
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
    
    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
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


static struct SceneScriptBindings : ScriptBindingsBase
{
    SceneScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<Scene>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<Handle<Scene>>(
                "Scene",
                {
                    API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< Handle<Scene>, void *, ScriptCreateObject<Scene> >
                    ),
                    API::NativeMemberDefine(
                        "GetID",
                        BuiltinTypes::UNSIGNED_INT,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< UInt32, const Handle<Scene> &, ScriptGetHandleIDValue<Scene> >
                    ),
                    API::NativeMemberDefine(
                        "Init",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFnWrapped< void, Handle<Scene>, Scene, &Scene::Init >
                    )
                }
            );
    }
} scene_script_bindings = { };

} // namespace hyperion::v2
