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

#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderGlobalState.hpp>

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

namespace hyperion {

#pragma region SceneValidation

static SceneValidationResult MergeSceneValidationResults(
    const SceneValidationResult& result_a,
    const SceneValidationResult& result_b)
{
    SceneValidationResult result;

    if (result_a.HasError())
    {
        result = result_a;
    }

    if (result_b.HasError())
    {
        if (result.HasError())
        {
            result = HYP_MAKE_ERROR(SceneValidationError, "{}\n{}", result_a.GetError().GetMessage(), result_b.GetError().GetMessage());
        }
        else
        {
            result = result_b;
        }
    }

    return result;
}

static SceneValidationResult ValidateSceneLights(const Scene* scene)
{
    auto validate_multiple_directional_lights = [](const Scene* scene) -> SceneValidationResult
    {
        int num_directional_lights = 0;

        for (auto [entity_id, light_component] : scene->GetEntityManager()->GetEntitySet<LightComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!light_component.light.IsValid())
            {
                continue;
            }

            if (light_component.light->GetLightType() == LightType::DIRECTIONAL)
            {
                ++num_directional_lights;
            }
        }

        if (num_directional_lights > 1)
        {
            return HYP_MAKE_ERROR(SceneValidationError, "Multiple directional lights found in scene");
        }

        return {};
    };

    SceneValidationResult result;
    result = MergeSceneValidationResults(result, validate_multiple_directional_lights(scene));

    return result;
}

SceneValidationResult SceneValidation::ValidateScene(const Scene* scene)
{
    SceneValidationResult result;
    result = MergeSceneValidationResults(result, ValidateSceneLights(scene));

    return result;
}

#pragma endregion SceneValidation

#pragma region Scene

Scene::Scene()
    : Scene(nullptr, ThreadID::Current(), {})
{
}

Scene::Scene(EnumFlags<SceneFlags> flags)
    : Scene(nullptr, ThreadID::Current(), flags)
{
}

Scene::Scene(World* world, EnumFlags<SceneFlags> flags)
    : Scene(world, ThreadID::Current(), flags)
{
}

Scene::Scene(World* world, ThreadID owner_thread_id, EnumFlags<SceneFlags> flags)
    : m_name(Name::Unique("Scene")),
      m_flags(flags),
      m_owner_thread_id(owner_thread_id),
      m_world(world),
      m_is_audio_listener(false),
      m_entity_manager(CreateObject<EntityManager>(owner_thread_id, this)),
      m_octree(m_entity_manager, BoundingBox(Vec3f(-250.0f), Vec3f(250.0f))),
      m_previous_delta(0.01667f),
      m_render_resource(nullptr)
{
    // Disable command execution if not on game thread
    if (m_owner_thread_id != g_game_thread)
    {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() & ~EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    }

    m_root = CreateObject<Node>(NAME("<ROOT>"), Handle<Entity>::empty, Transform::identity, this);
}

Scene::~Scene()
{
    m_octree.SetEntityManager(nullptr);
    m_octree.Clear();

    if (m_root.IsValid())
    {
        if (m_owner_thread_id.IsValid() && !Threads::IsOnThread(m_owner_thread_id))
        {
            Task<void> task = Threads::GetThread(m_owner_thread_id)->GetScheduler().Enqueue([&node = m_root]()
                {
                    node->SetScene(nullptr);
                });

            task.Await();
        }
        else
        {
            m_root->SetScene(nullptr);
        }
    }

    // Move so destruction of components can check GetEntityManager() returns nullptr
    if (Handle<EntityManager> entity_manager = std::move(m_entity_manager))
    {
        if (Threads::IsOnThread(entity_manager->GetOwnerThreadID()))
        {
            // If we are on the same thread, we can safely shutdown the entity manager here:
            entity_manager->Shutdown();
        }
        else
        {
            // have to enqueue a task to shut down the entity manager on its owner thread
            Task<void> task = Threads::GetThread(entity_manager->GetOwnerThreadID())->GetScheduler().Enqueue([&entity_manager]()
                {
                    entity_manager->Shutdown();
                });

            // Wait for shut down to complete
            task.Await();
        }

        entity_manager.Reset();
    }

    if (m_render_resource != nullptr)
    {
        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }
}

void Scene::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    m_entity_manager->SetWorld(m_world);

    m_render_resource = AllocateResource<RenderScene>(this);

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

    InitObject(m_entity_manager);
}

void Scene::SetOwnerThreadID(ThreadID owner_thread_id)
{
    if (m_owner_thread_id == owner_thread_id)
    {
        return;
    }

    m_owner_thread_id = owner_thread_id;
    m_entity_manager->SetOwnerThreadID(owner_thread_id);

    if (m_owner_thread_id == g_game_thread)
    {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() | EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    }
    else
    {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() & ~EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    }
}

const Handle<Camera>& Scene::GetPrimaryCamera() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!m_entity_manager)
    {
        return Handle<Camera>::empty;
    }

    for (auto [entity_id, camera_component, _] : m_entity_manager->GetEntitySet<CameraComponent, EntityTagComponent<EntityTag::CAMERA_PRIMARY>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (camera_component.camera.IsValid())
        {
            return camera_component.camera;
        }
    }

    return Handle<Camera>::empty;
}

void Scene::SetWorld(World* world)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (m_world == world)
    {
        return;
    }

    if (m_world != nullptr && m_world->HasScene(GetID()))
    {
        m_world->RemoveScene(HandleFromThis());
    }

    // When world is changed, entity manager needs all systems to have this change reflected
    m_entity_manager->SetWorld(world);

    m_world = world;
}

Handle<Node> Scene::FindNodeWithEntity(ID<Entity> entity) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertThrow(m_root);

    if (m_root->GetEntity() == entity)
    {
        return m_root;
    }

    return m_root->FindChildWithEntity(entity);
}

Handle<Node> Scene::FindNodeByName(WeakName name) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertThrow(m_root);

    if (m_root->GetName() == name)
    {
        return m_root;
    }

    return m_root->FindChildByName(name);
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

    if (IsForegroundScene())
    {
        m_entity_manager->FlushCommandQueue(delta);
    }
}

void Scene::SetRoot(const Handle<Node>& root)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (root == m_root)
    {
        return;
    }

#ifdef HYP_DEBUG_MODE
    RemoveDelegateHandler(NAME("ValidateScene"));
#endif

    Handle<Node> prev_root = m_root;

    if (prev_root.IsValid() && prev_root->GetScene() == this)
    {
        prev_root->SetScene(nullptr);
    }

    m_root = root;

    if (m_root.IsValid())
    {
        m_root->SetScene(this);

#ifdef HYP_DEBUG_MODE
        AddDelegateHandler(
            NAME("ValidateScene"),
            m_root->GetDelegates()->OnChildAdded.Bind([weak_this = WeakHandleFromThis()](Node*, bool)
                {
                    Handle<Scene> scene = weak_this.Lock();

                    if (!scene.IsValid())
                    {
                        return;
                    }

                    SceneValidationResult validation_result = SceneValidation::ValidateScene(scene.Get());

                    if (validation_result.HasError())
                    {
                        HYP_LOG(Scene, Error, "Scene validation failed: {}", validation_result.GetError().GetMessage());
                    }
                }));
#endif
    }

    OnRootNodeChanged(m_root, prev_root);
}

bool Scene::AddToWorld(World* world)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    if (world == m_world)
    {
        // World is same, just return true
        return true;
    }

    if (m_world != nullptr)
    {
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

    if (m_world == nullptr)
    {
        return false;
    }

    m_world->RemoveScene(HandleFromThis());

    return true;
}

Name Scene::GetUniqueNodeName(UTF8StringView base_name) const
{
    String unique_name = base_name;
    int counter = 1;

    // Return base_name directly if it's not already used.
    if (!FindNodeByName(unique_name).IsValid())
    {
        return CreateNameFromDynamicString(unique_name);
    }

    // Otherwise, append an increasing counter until a unique name is found.
    while (FindNodeByName(unique_name).IsValid())
    {
        unique_name = HYP_FORMAT("{}{}", base_name, counter);
        ++counter;
    }

    return CreateNameFromDynamicString(unique_name);
}

template <class SystemType>
void Scene::AddSystemIfApplicable()
{
    Handle<SystemType> system = CreateObject<SystemType>(*m_entity_manager);

    if (!system->ShouldCreateForScene(this))
    {
        return;
    }

    m_entity_manager->AddSystem(system);
}

#pragma endregion Scene

} // namespace hyperion
