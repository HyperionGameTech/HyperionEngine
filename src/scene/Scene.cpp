/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Light.hpp>

#include <scene/EntityManager.hpp>

#include <scene/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/systems/EntityRenderProxySystem_Mesh.hpp>
#include <scene/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/systems/AnimationSystem.hpp>
#include <scene/systems/LightmapSystem.hpp>
#include <scene/systems/SkySystem.hpp>
#include <scene/systems/AudioSystem.hpp>
#include <scene/systems/PhysicsSystem.hpp>
#include <scene/systems/ScriptSystem.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/rt/RenderAccelerationStructure.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/math/Halton.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG

namespace hyperion {

#pragma region SceneValidation

static SceneValidationResult MergeSceneValidationResults(
    const SceneValidationResult& resultA,
    const SceneValidationResult& resultB)
{
    SceneValidationResult result;

    if (resultA.HasError())
    {
        result = resultA;
    }

    if (resultB.HasError())
    {
        if (result.HasError())
        {
            result = HYP_MAKE_ERROR(SceneValidationError, "{}\n{}", resultA.GetError().GetMessage(), resultB.GetError().GetMessage());
        }
        else
        {
            result = resultB;
        }
    }

    return result;
}

static SceneValidationResult ValidateSceneLights(const Scene* scene)
{
    auto validateMultipleDirectionalLights = [](const Scene* scene) -> SceneValidationResult
    {
        int numDirectionalLights = 0;

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Light* light = ObjCast<Light>(entity);

            if (light->GetLightType() == LT_DIRECTIONAL)
            {
                ++numDirectionalLights;
            }
        }

        if (numDirectionalLights > 1)
        {
            return HYP_MAKE_ERROR(SceneValidationError, "Multiple directional lights found in scene");
        }

        return {};
    };

    SceneValidationResult result;
    result = MergeSceneValidationResults(result, validateMultipleDirectionalLights(scene));

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
    : Scene(nullptr, ThreadId::Current(), {})
{
}

Scene::Scene(EnumFlags<SceneFlags> flags)
    : Scene(nullptr, ThreadId::Current(), flags)
{
}

Scene::Scene(World* world, EnumFlags<SceneFlags> flags)
    : Scene(world, ThreadId::Current(), flags)
{
}

Scene::Scene(World* world, ThreadId ownerThreadId, EnumFlags<SceneFlags> flags)
    : m_name(Name::Unique("Scene")),
      m_flags(flags),
      m_ownerThreadId(ownerThreadId),
      m_world(world),
      m_isAudioListener(false),
      m_entityManager(CreateObject<EntityManager>(ownerThreadId, this)),
      m_octree(m_entityManager, BoundingBox(Vec3f(-250.0f), Vec3f(250.0f))),
      m_previousDelta(0.01667f)
{
    m_root = CreateObject<Node>(NAME("<ROOT>"), Transform::identity, this);
}

Scene::~Scene()
{
    m_octree.SetEntityManager(nullptr);
    m_octree.Clear();

    if (m_root.IsValid())
    {
        if (m_ownerThreadId.IsValid() && !Threads::IsOnThread(m_ownerThreadId))
        {
            Task<void> task = Threads::GetThread(m_ownerThreadId)->GetScheduler().Enqueue([&node = m_root]()
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
    if (Handle<EntityManager> entityManager = std::move(m_entityManager))
    {
        if (Threads::IsOnThread(entityManager->GetOwnerThreadId()))
        {
            // If we are on the same thread, we can safely shutdown the entity manager here:
            entityManager->Shutdown();
        }
        else
        {
            // have to enqueue a task to shut down the entity manager on its owner thread
            Task<void> task = Threads::GetThread(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue([&entityManager]()
                {
                    entityManager->Shutdown();
                });

            // Wait for shut down to complete
            task.Await();
        }

        entityManager.Reset();
    }
}

void Scene::Init()
{
    m_entityManager->SetWorld(m_world);

    AddSystemIfApplicable<WorldAABBUpdaterSystem>();
    AddSystemIfApplicable<EntityMeshDirtyStateSystem>();
    AddSystemIfApplicable<EntityRenderProxySystem_Mesh>();
    AddSystemIfApplicable<VisibilityStateUpdaterSystem>();
    AddSystemIfApplicable<LightmapSystem>();
    AddSystemIfApplicable<AnimationSystem>();
    AddSystemIfApplicable<SkySystem>();
    AddSystemIfApplicable<AudioSystem>();
    AddSystemIfApplicable<PhysicsSystem>();
    AddSystemIfApplicable<ScriptSystem>();

    // Scene must be ready before entity manager is initialized
    // (OnEntityAdded() calls on Systems may require this)
    SetReady(true);

    InitObject(m_root);
    InitObject(m_entityManager);

    AssertDebug(m_entityManager->GetWorld() == m_world);
}

void Scene::SetOwnerThreadId(ThreadId ownerThreadId)
{
    if (m_ownerThreadId == ownerThreadId)
    {
        return;
    }

    m_ownerThreadId = ownerThreadId;
    m_entityManager->SetOwnerThreadId(ownerThreadId);
}

Camera* Scene::GetPrimaryCamera() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!m_entityManager)
    {
        return nullptr;
    }

    for (auto [entity, _0, _1] : m_entityManager->GetEntitySet<EntityType<Camera>, EntityTagComponent<EntityTag::CAMERA_PRIMARY>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        AssertDebug(entity->IsA<Camera>());

        return static_cast<Camera*>(entity);
    }

    return nullptr;
}

void Scene::SetWorld(World* world)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    if (m_world == world)
    {
        return;
    }

    if (m_world != nullptr && m_world->HasScene(Id()))
    {
        m_world->RemoveScene(HandleFromThis());
    }
    
    if (world != nullptr)
    {
        AssertDebug(m_world == nullptr, "Setting Scene {} (name: {}) world to {} but already has world {} set",
                    Id(), GetName(), world->Id(), m_world->Id());
    }

    // When world is changed, entity manager needs all systems to have this change reflected
    m_entityManager->SetWorld(world);

    m_world = world;
}

Handle<Node> Scene::FindNodeByName(WeakName name) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    Assert(m_root);

    if (m_root->GetName() == name)
    {
        return m_root;
    }

    return m_root->FindChildByName(name);
}

void Scene::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    AssertReady();

    {
        HYP_NAMED_SCOPE("Update octree");

        // Rebuild any octants that have had structural changes
        // IMPORTANT: must be ran at start of tick, as pointers to octants' visibility states will be
        // stored on VisibilityStateComponent.
        m_octree.PerformUpdates();
        m_octree.NextVisibilityState();
    }
}

void Scene::SetRoot(const Handle<Node>& root)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    if (root == m_root)
    {
        return;
    }

#ifdef HYP_DEBUG_MODE
    RemoveDelegateHandler(NAME("ValidateScene"));
#endif

    Handle<Node> prevRoot = m_root;

    if (prevRoot.IsValid() && prevRoot->GetScene() == this)
    {
        prevRoot->SetScene(nullptr);
    }

    m_root = root;

    if (m_root.IsValid())
    {
        m_root->SetScene(this);

#ifdef HYP_DEBUG_MODE
        AddDelegateHandler(
            NAME("ValidateScene"),
            m_root->GetDelegates()->OnChildAdded.Bind([weakThis = WeakHandleFromThis()](Node*, bool)
                {
                    Handle<Scene> scene = weakThis.Lock();

                    if (!scene.IsValid())
                    {
                        return;
                    }

                    SceneValidationResult validationResult = SceneValidation::ValidateScene(scene.Get());

                    if (validationResult.HasError())
                    {
                        HYP_LOG(Scene, Error, "Scene validation failed: {}", validationResult.GetError().GetMessage());
                    }
                }));
#endif
    }

    OnRootNodeChanged(m_root, prevRoot);
}

bool Scene::AddToWorld(World* world)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

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
    Threads::AssertOnThread(g_gameThread);

    if (m_world == nullptr)
    {
        return false;
    }

    m_world->RemoveScene(HandleFromThis());

    return true;
}

Name Scene::GetUniqueNodeName(UTF8StringView baseName) const
{
    String uniqueName = baseName;
    int counter = 1;

    // Return baseName directly if it's not already used.
    if (!FindNodeByName(uniqueName).IsValid())
    {
        return CreateNameFromDynamicString(uniqueName);
    }

    // Otherwise, append an increasing counter until a unique name is found.
    while (FindNodeByName(uniqueName).IsValid())
    {
        uniqueName = HYP_FORMAT("{}{}", baseName, counter);
        ++counter;
    }

    return CreateNameFromDynamicString(uniqueName);
}

template <class SystemType>
void Scene::AddSystemIfApplicable()
{
    Handle<SystemType> system = CreateObject<SystemType>(*m_entityManager);

    if (!system->ShouldCreateForScene(this))
    {
        return;
    }

    m_entityManager->AddSystem(system);
}

#pragma endregion Scene

} // namespace hyperion
