#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/ReflectionProbeRenderer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <math/Halton.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>

// New ECS
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/SceneComponent.hpp>
#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/systems/EntityDrawDataUpdaterSystem.hpp>
#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/systems/AnimationSystem.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG
// #define HYP_DISABLE_VISIBILITY_CHECK

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

// static void CollectOctantEntities(
//     RenderList &render_list,
//     const Handle<Camera> &camera,
//     RenderableAttributeSet *override_attributes_ptr,
//     const Octree *octree
// )
// {
//     if (!Octree::IsVisible(&g_engine->GetWorld()->GetOctree(), octree)) {
//         return;
//     }

//     for (const Octree::Node &octree_node : octree->GetNodes()) {
//         const Entity *entity = octree_node.entity;

//         if (!entity) {
//             continue;
//         }

//         if (entity->IsRenderable()) {
//             render_list.PushEntityToRender(camera, Handle<Entity>(entity->GetID()) /* temp */, override_attributes_ptr);
//         }
//     }

//     if (octree->IsDivided()) {
//         for (const Octree::Octant &octant : octree->GetOctants()) {
//             CollectOctantEntities(
//                 render_list,
//                 camera,
//                 override_attributes_ptr,
//                 octant.octree.Get()
//             );
//         }
//     }
// }

Scene::Scene()
    : Scene(Handle<Camera>::empty, { })
{
}

Scene::Scene(Handle<Camera> camera)
    : Scene(std::move(camera), { })
{
}

Scene::Scene(
    Handle<Camera> camera,
    const InitInfo &info
) : BasicObject(info),
    HasDrawProxy(),
    m_camera(std::move(camera)),
    m_root_node_proxy(new Node("root")),
    m_environment(new RenderEnvironment(this)),
    m_world(nullptr),
    m_is_non_world_scene(false),
    m_entity_manager(new EntityManager(this)),
    m_octree(m_entity_manager, BoundingBox(Vec3f(-250.0f), Vec3f(250.0f))),
    m_shader_data_state(ShaderDataState::DIRTY)
{
    m_entity_manager->AddSystem<WorldAABBUpdaterSystem>();
    m_entity_manager->AddSystem<VisibilityStateUpdaterSystem>();
    m_entity_manager->AddSystem<EntityDrawDataUpdaterSystem>();
    m_entity_manager->AddSystem<LightVisibilityUpdaterSystem>();
    m_entity_manager->AddSystem<ShadowMapUpdaterSystem>();
    m_entity_manager->AddSystem<AnimationSystem>();

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
    
    BasicObject::Init();

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

    // if (m_lights.Any()) {
    //     // enqueue bind for all in bulk
    //     auto *lights = new Pair<ID<Light>, LightDrawProxy>[m_lights.Size()];
    //     SizeType index = 0;

    //     for (auto &it : m_lights) {
    //         auto &light = it.second;
    //         AssertThrow(InitObject(light));

    //         lights[index++] = { it.first, it.second->GetDrawProxy() };
    //     }

    //     PUSH_RENDER_COMMAND(BindLights, 
    //         index,
    //         lights
    //     );
    // }

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

        HYP_SYNC_RENDER();

        SetReady(false);
    });
}

void Scene::SetCamera(Handle<Camera> camera)
{
    m_camera = std::move(camera);
    InitObject(m_camera);

    m_render_list.SetCamera(m_camera);
}

void Scene::SetWorld(World *world)
{
    // be cautious
    // Threads::AssertOnThread(THREAD_GAME);

    m_world = world;
}

NodeProxy Scene::FindNodeWithEntity(ID<Entity> entity) const
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_root_node_proxy);
    return m_root_node_proxy.Get()->FindChildWithEntity(entity);
}

NodeProxy Scene::FindNodeByName(const String &name) const
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_root_node_proxy);
    return m_root_node_proxy.Get()->FindChildByName(name);
}

// Bool Scene::AddLight(Handle<Light> &&light)
// {
//     Threads::AssertOnThread(THREAD_GAME);

//     if (!light) {
//         return false;
//     }

//     auto insert_result = m_lights.Insert(light->GetID(), std::move(light));
    
//     auto it = insert_result.first;
//     const Bool was_inserted = insert_result.second;

//     if (!was_inserted) {
//         return false;
//     }

//     InitObject(it->second);

//     return true;
// }

// Bool Scene::AddLight(const Handle<Light> &light)
// {
//     Threads::AssertOnThread(THREAD_GAME);

//     if (!light) {
//         return false;
//     }

//     auto insert_result = m_lights.Insert(light->GetID(), light);
    
//     auto it = insert_result.first;
//     const Bool was_inserted = insert_result.second;

//     if (!was_inserted) {
//         return false;
//     }

//     InitObject(it->second);

//     return true;
// }

// Bool Scene::RemoveLight(ID<Light> id)
// {
//     Threads::AssertOnThread(THREAD_GAME);

//     auto it = m_lights.Find(id);

//     if (it == m_lights.End()) {
//         return false;
//     }

//     if (it->second) {
//         it->second->EnqueueUnbind();
//     }

//     return m_lights.Erase(it) != m_lights.End();
// }

Bool Scene::AddEnvProbe(Handle<EnvProbe> env_probe)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (!env_probe) {
        return false;
    }

    auto insert_result = m_env_probes.Insert(env_probe->GetID(), std::move(env_probe));
    
    auto it = insert_result.first;
    const Bool was_inserted = insert_result.second;

    if (!was_inserted) {
        return false;
    }

    if (InitObject(it->second)) {
        it->second->EnqueueBind();
    }

    return true;
}

Bool Scene::RemoveEnvProbe(ID<EnvProbe> id)
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

void Scene::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_octree.NextVisibilityState();

    ID<Camera> camera_id;

    if (m_camera) {
        camera_id = m_camera->GetID();

        m_camera->Update(delta);

        // update octree visibility states using the camera
        m_octree.CalculateVisibility(m_camera.Get());

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    m_entity_manager->Update(delta);

    EnqueueRenderUpdates();

    // update EnvProbe visibility states
    for (auto &it : m_env_probes) {
        Handle<EnvProbe> &env_probe = it.second;

        const Bool is_env_probe_in_frustum = m_camera.IsValid() && m_camera->GetFrustum().ContainsAABB(env_probe->GetAABB());

        env_probe->SetIsVisible(camera_id, is_env_probe_in_frustum);
    }

    if (IsWorldScene()) {
        // update render environment
        m_environment->Update(delta);

        // update each environment probe
        for (auto &it : m_env_probes) {
            Handle<EnvProbe> &env_probe = it.second;

            env_probe->Update(delta);
        }
    }
}

void Scene::CollectEntities(
    RenderList &render_list,
    const Handle<Camera> &camera,
    const Bitset &bucket_bits,
    Optional<RenderableAttributeSet> override_attributes,
    Bool skip_frustum_culling
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
    
    const UInt8 visibility_cursor = m_octree.LoadVisibilityCursor();

    // @TODO: Update this to use the new ECS

    // push all entities to render if they are visible to the given camera
    // for (auto &it : m_entities) {
    //     const Handle<Entity> &entity = it.second;

    //     if (
    //         entity->IsRenderable()
    //         && bucket_bits.Test(entity->GetRenderableAttributes().GetMaterialAttributes().bucket)
    //         && (skip_frustum_culling || IsEntityInFrustum(entity, camera_id, visibility_cursor))
    //     ) {
    //         render_list.PushEntityToRender(camera, entity, override_attributes_ptr);
    //     }
    // }
}

void Scene::CollectEntities(
    RenderList &render_list,
    const Handle<Camera> &camera,
    Optional<RenderableAttributeSet> override_attributes,
    Bool skip_frustum_culling
) const
{
    Threads::AssertOnThread(THREAD_GAME | THREAD_TASK);

    // clear out existing entities before populating
    render_list.ClearEntities();

    if (!camera) {
        return;
    }

    const ID<Camera> camera_id = camera->GetID();

    RenderableAttributeSet *override_attributes_ptr = override_attributes.TryGet();
    const UInt32 override_flags = override_attributes_ptr ? override_attributes_ptr->GetOverrideFlags() : 0;
    
    const UInt8 visibility_cursor = m_octree.LoadVisibilityCursor();
    const VisibilityState &parent_visibility_state = m_octree.GetVisibilityState();

    UInt num_collected = 0;

    for (auto it : m_entity_manager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent>()) {
        auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component] = it;

        // Temp
        AssertThrow(mesh_component.material.IsValid());
        AssertThrow(mesh_component.material->GetRenderAttributes().shader_definition.IsValid());

        // hack
        // if (!mesh_component.material->GetRenderAttributes().shader_definition.IsValid()) {
        //     continue;
        // }

        if (!skip_frustum_culling) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            // Visibility check
            if (!visibility_state_component.visibility_state.ValidToParent(parent_visibility_state, visibility_cursor)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                DebugLog(
                    LogType::Debug,
                    "Skipping entity #%u due to visibility state being invalid.\n",
                    entity_id.Value()
                );
#endif

                continue;
            }

            if (!visibility_state_component.visibility_state.Get(camera_id, visibility_cursor)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                DebugLog(
                    LogType::Debug,
                    "Skipping entity #%u due to visibility state being false\n",
                    entity_id.Value()
                );
#endif

                continue;
            }
#endif
        }

        render_list.PushEntityToRender(
            camera,
            entity_id,
            mesh_component.mesh,
            mesh_component.material,
            Handle<Skeleton>::empty, // TEMP
            transform_component.transform.GetMatrix(),
            mesh_component.previous_model_matrix,
            bounding_box_component.world_aabb,
            override_attributes_ptr
        );

        num_collected++;
    }

    // DebugLog(
    //     LogType::Debug,
    //     "Collected %u entities\n",
    //     num_collected
    // );
}

Bool Scene::IsEntityInFrustum(const Handle<Entity> &entity, ID<Camera> camera_id, UInt8 visibility_cursor) const
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
        FogParams fog_params;
        RenderEnvironment *render_environment;
        SceneDrawProxy &draw_proxy;

        RENDER_COMMAND(UpdateSceneRenderData)(
            ID<Scene> id,
            const BoundingBox &aabb,
            Float global_timer,
            const FogParams &fog_params,
            RenderEnvironment *render_environment,
            SceneDrawProxy &draw_proxy
        ) : id(id),
            aabb(aabb),
            global_timer(global_timer),
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
        m_fog_params,
        m_environment.Get(),
        m_draw_proxy
    );

    m_shader_data_state = ShaderDataState::CLEAN;
}

Bool Scene::CreateTLAS()
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
