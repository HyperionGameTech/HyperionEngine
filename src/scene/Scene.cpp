/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Light.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <scene/ecs/systems/CameraSystem.hpp>
#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/systems/RenderProxyUpdaterSystem.hpp>
#include <scene/ecs/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/systems/ReflectionProbeUpdaterSystem.hpp>
#include <scene/ecs/systems/AnimationSystem.hpp>
#include <scene/ecs/systems/LightmapSystem.hpp>
#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/systems/AudioSystem.hpp>
#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/systems/BVHUpdaterSystem.hpp>
#include <scene/ecs/systems/PhysicsSystem.hpp>
#include <scene/ecs/systems/ScriptSystem.hpp>
#include <scene/ecs/systems/ScenePrimaryCameraSystem.hpp>

#include <scene/world_grid/WorldGridSubsystem.hpp>

#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/math/Halton.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG
// #define HYP_DISABLE_VISIBILITY_CHECK

namespace hyperion {

#pragma region SceneValidation

static SceneValidationResult MergeSceneValidationResults(
    const SceneValidationResult &result_a,
    const SceneValidationResult &result_b
)
{
    SceneValidationResult result;

    if (result_a.HasError()) {
        result = result_a;
    }

    if (result_b.HasError()) {
        if (result.HasError()) {
            result = HYP_MAKE_ERROR(SceneValidationError, "{}\n{}", result_a.GetError().GetMessage(), result_b.GetError().GetMessage());
        } else {
            result = result_b;
        }
    }

    return result;
}

static SceneValidationResult ValidateSceneLights(const Scene *scene)
{
    auto ValidateMultipleDirectionalLights = [](const Scene *scene) -> SceneValidationResult
    {
        int num_directional_lights = 0;

        for (auto [entity_id, light_component] : scene->GetEntityManager()->GetEntitySet<LightComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
            if (!light_component.light.IsValid()) {
                continue;
            }

            if (light_component.light->GetLightType() == LightType::DIRECTIONAL) {
                ++num_directional_lights;
            }
        }

        if (num_directional_lights > 1) {
            return HYP_MAKE_ERROR(SceneValidationError, "Multiple directional lights found in scene");
        }

        return { };
    };

    SceneValidationResult result;
    result = MergeSceneValidationResults(result, ValidateMultipleDirectionalLights(scene));

    return result;
}

SceneValidationResult SceneValidation::ValidateScene(const Scene *scene)
{
    SceneValidationResult result;
    result = MergeSceneValidationResults(result, ValidateSceneLights(scene));

    return result;
}

#pragma endregion SceneValidation

#pragma region Scene

Scene::Scene()
    : Scene(nullptr, g_game_thread, { })
{
}
    
Scene::Scene(EnumFlags<SceneFlags> flags)
    : Scene(nullptr, g_game_thread, flags)
{
}

Scene::Scene(World *world, EnumFlags<SceneFlags> flags)
    : Scene(world, g_game_thread, flags)
{
}

Scene::Scene(
    World *world,
    ThreadID owner_thread_id,
    EnumFlags<SceneFlags> flags
) : m_name(Name::Unique("Scene")),
    m_flags(flags),
    m_owner_thread_id(owner_thread_id),
    m_world(world),
    m_is_audio_listener(false),
    m_entity_manager(MakeRefCountedPtr<EntityManager>(owner_thread_id, this)),
    m_octree(m_entity_manager, BoundingBox(Vec3f(-250.0f), Vec3f(250.0f))),
    m_previous_delta(0.01667f),
    m_render_resource(nullptr)
{
    // Disable command execution if not on game thread
    if (m_owner_thread_id != g_game_thread) {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() & ~EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    }

    SetRoot(MakeRefCountedPtr<Node>("<ROOT>", Handle<Entity>::empty, Transform::identity, this));
}

Scene::~Scene()
{
    m_octree.SetEntityManager(nullptr);
    m_octree.Clear();

    if (m_root_node_proxy.IsValid()) {
        m_root_node_proxy->SetScene(nullptr);
    }

    // Move so destruction of components can check GetEntityManager() returns nullptr
    RC<EntityManager> entity_manager = std::move(m_entity_manager);
    entity_manager.Reset();

    if (m_render_resource != nullptr) {
        if (m_world != nullptr && m_world->IsReady()) {
            m_world->GetRenderResource().RemoveViewsForScene(HandleFromThis());
        }

        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }
}
    
void Scene::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        if (m_render_resource != nullptr) {
            if (m_world != nullptr && m_world->IsReady()) {
                m_world->GetRenderResource().RemoveViewsForScene(HandleFromThis());
            }

            FreeResource(m_render_resource);

            m_render_resource = nullptr;
        }
    }));

    m_render_resource = AllocateResource<SceneRenderResource>(this);

    AddSystemIfApplicable<CameraSystem>();
    AddSystemIfApplicable<ScenePrimaryCameraSystem>();
    AddSystemIfApplicable<WorldAABBUpdaterSystem>();
    AddSystemIfApplicable<EntityMeshDirtyStateSystem>();
    AddSystemIfApplicable<RenderProxyUpdaterSystem>();
    AddSystemIfApplicable<VisibilityStateUpdaterSystem>();
    AddSystemIfApplicable<ReflectionProbeUpdaterSystem>();
    AddSystemIfApplicable<LightVisibilityUpdaterSystem>();
    AddSystemIfApplicable<ShadowMapUpdaterSystem>();
    AddSystemIfApplicable<EnvGridUpdaterSystem>();
    AddSystemIfApplicable<LightmapSystem>();
    AddSystemIfApplicable<AnimationSystem>();
    AddSystemIfApplicable<SkySystem>();
    AddSystemIfApplicable<AudioSystem>();
    AddSystemIfApplicable<BLASUpdaterSystem>();
    AddSystemIfApplicable<BVHUpdaterSystem>();
    AddSystemIfApplicable<PhysicsSystem>();
    AddSystemIfApplicable<ScriptSystem>();

    // Scene must be ready before entity manager is initialized
    // (OnEntityAdded() calls on Systems may require this)
    SetReady(true);

    m_entity_manager->Initialize();
}

void Scene::SetOwnerThreadID(ThreadID owner_thread_id)
{
    if (m_owner_thread_id == owner_thread_id) {
        return;
    }

    m_owner_thread_id = owner_thread_id;
    m_entity_manager->SetOwnerThreadID(owner_thread_id);

    if (m_owner_thread_id == g_game_thread) {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() | EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    } else {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() & ~EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    }
}

const Handle<Camera> &Scene::GetPrimaryCamera() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!m_entity_manager) {
        return Handle<Camera>::empty;
    }

    for (auto [entity_id, camera_component, _] : m_entity_manager->GetEntitySet<CameraComponent, EntityTagComponent<EntityTag::CAMERA_PRIMARY>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (camera_component.camera.IsValid()) {
            return camera_component.camera;
        }
    }

    return Handle<Camera>::empty;
}

void Scene::SetWorld(World *world)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (m_world == world) {
        return;
    }

    if (m_world != nullptr && m_world->HasScene(GetID())) {
        m_world->RemoveScene(HandleFromThis());
    }

    // When world is changed, entity manager needs all systems to have this change reflected
    m_entity_manager->SetWorld(world);

    m_world = world;
}

WorldGrid *Scene::GetWorldGrid() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (!m_world) {
        return nullptr;
    }

    if (WorldGridSubsystem *world_grid_subsystem = m_world->GetSubsystem<WorldGridSubsystem>()) {
        return world_grid_subsystem->GetWorldGrid(GetID());
    }

    return nullptr;
}

NodeProxy Scene::FindNodeWithEntity(ID<Entity> entity) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertThrow(m_root_node_proxy);

    if (m_root_node_proxy->GetEntity() == entity) {
        return m_root_node_proxy;
    }

    return m_root_node_proxy->FindChildWithEntity(entity);
}

NodeProxy Scene::FindNodeByName(UTF8StringView name) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertThrow(m_root_node_proxy);

    if (m_root_node_proxy->GetName() == name) {
        return m_root_node_proxy;
    }

    return m_root_node_proxy->FindChildByName(name);
}

void Scene::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertReady();

    {
        HYP_NAMED_SCOPE("Update octree");

        // Rebuild any octants that have had structural changes
        // IMPORTANT: must be ran at start of tick, as pointers to octants' visibility states will be
        // stored on VisibilityStateComponent.
        m_octree.PerformUpdates();
        m_octree.NextVisibilityState();
    }

    if (IsForegroundScene()) {
        m_entity_manager->FlushCommandQueue(delta);
    }
}

void Scene::EnqueueRenderUpdates()
{
    HYP_SCOPE;

    AssertReady();

    SceneShaderData shader_data { };
    shader_data.fog_params = Vec4f(float(m_fog_params.color.Packed()), m_fog_params.start_distance, m_fog_params.end_distance, 0.0f);
    shader_data.game_time = m_world != nullptr ? m_world->GetGameState().game_time : 0.0f;

    m_render_resource->SetBufferData(shader_data);
}

void Scene::SetRoot(const NodeProxy &root)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (root == m_root_node_proxy) {
        return;
    }

#ifdef HYP_DEBUG_MODE
    RemoveDelegateHandler(NAME("ValidateScene"));
#endif

    NodeProxy prev_root = m_root_node_proxy;

    if (prev_root.IsValid() && prev_root->GetScene() == this) {
        prev_root->SetScene(nullptr);
    }

    m_root_node_proxy = root;

    if (m_root_node_proxy.IsValid()) {
        m_root_node_proxy->SetScene(this);

#ifdef HYP_DEBUG_MODE
        AddDelegateHandler(NAME("ValidateScene"), m_root_node_proxy->GetDelegates()->OnChildAdded.Bind([weak_this = WeakHandleFromThis()](Node *, bool)
        {
            Handle<Scene> scene = weak_this.Lock();

            if (!scene.IsValid()) {
                return;
            }

            SceneValidationResult validation_result = SceneValidation::ValidateScene(scene.Get());

            if (validation_result.HasError()) {
                HYP_LOG(Scene, Error, "Scene validation failed: {}", validation_result.GetError().GetMessage());
            }
        }));
#endif
    }

    OnRootNodeChanged(m_root_node_proxy, prev_root);
}

bool Scene::AddToWorld(World *world)
{
    HYP_SCOPE;
    
    Threads::AssertOnThread(g_game_thread);

    if (world == m_world) {
        // World is same, just return true
        return true;
    }

    if (m_world != nullptr) {
        // Can't add to world, world already set
        return false;
    }

    world->AddScene(HandleFromThis());

    return true;
}

bool Scene::RemoveFromWorld()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (m_world == nullptr) {
        return false;
    }

    m_world->RemoveScene(HandleFromThis());

    return true;
}

String Scene::GetUniqueNodeName(UTF8StringView base_name) const
{
    String unique_name = base_name;
    int counter = 1;

    // Return base_name directly if it's not already used.
    if (!FindNodeByName(unique_name).IsValid()) {
        return unique_name;
    }
    
    // Otherwise, append an increasing counter until a unique name is found.
    while (FindNodeByName(unique_name).IsValid()) {
        unique_name = HYP_FORMAT("{}{}", base_name, counter);
        ++counter;
    }
    
    return unique_name;
}

template <class SystemType>
void Scene::AddSystemIfApplicable()
{
    UniquePtr<SystemType> system = MakeUnique<SystemType>(*m_entity_manager);

    if (!system->ShouldCreateForScene(this)) {
        return;
    }

    m_entity_manager->AddSystem(std::move(system));
}

#pragma endregion Scene

} // namespace hyperion
