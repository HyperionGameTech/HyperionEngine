/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Light.hpp>

#include <scene/EntityManager.hpp>

#include <rendering/RenderInterface.hpp>

#include <rendering/AccelerationStructure.hpp>

#include <system/AppContext.hpp>

#include <Core/math/Halton.hpp>

#include <engine/EngineDriver.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG

#include <Scene.generated.inl>

namespace Hyperion {

static const Name s_nameUnnamedScene = NAME("<unnamed scene>");
static const Name s_nameSceneRoot = NAME("<ROOT>");

void Scene_OnPostLoad(Scene& scene)
{
    scene.SetOwnerThreadId(g_simThread);
}

#pragma region Scene

Scene::Scene()
    : Scene(s_nameUnnamedScene, ThreadId::Current(), SceneFlags::DEFAULT)
{
}

Scene::Scene(EnumFlags<SceneFlags> flags)
    : Scene(s_nameUnnamedScene, ThreadId::Current(), flags)
{
}

Scene::Scene(Name name, EnumFlags<SceneFlags> flags)
    : Scene(name, ThreadId::Current(), flags)
{
}

Scene::Scene(Name name, ThreadId ownerThreadId, EnumFlags<SceneFlags> flags)
    : AssetObject(name),
      m_sceneFlags(flags),
      m_ownerThreadId(ownerThreadId),
      m_world(nullptr),
      m_entityManager(MakeHandle<EntityManager>(ownerThreadId, this)),
      m_octree(m_entityManager, BoundingBox(Vec3f(-250.0f), Vec3f(250.0f))),
      m_previousDelta(0.01667f)
{
    m_root = MakeHandle<Node>(s_nameSceneRoot, Transform::identity, this);
    m_root->SetIsStatic(false);
}

Scene::~Scene()
{
    m_octree.SetEntityManager(nullptr);
    m_octree.Clear();

    if (m_root.IsValid())
    {
        if (m_ownerThreadId.IsValid() && !IsOnThread(m_ownerThreadId))
        {
            Task<void> task = GetThreadById(m_ownerThreadId)->GetScheduler().Enqueue([&node = m_root]()
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
    if (Handle<EntityManager> entityManager = std::move(m_entityManager); entityManager.IsValid())
    {
        if (IsOnThread(entityManager->GetOwnerThreadId()))
        {
            // If we are on the same thread, we can safely shutdown the entity manager here:
            entityManager->Shutdown();
        }
        else
        {
            // have to enqueue a task to shut down the entity manager on its owner thread
            Task<void> task = GetThreadById(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue([&entityManager]()
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
    HYP_SCOPE;
    AssetObject::Init();

    m_entityManager->SetWorld(m_world);

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
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!m_entityManager)
    {
        return nullptr;
    }

    for (auto [entity, _0, _1] : m_entityManager->GetEntitySet<EntityType<Camera>, TagComponent<EntityTag::PrimaryCamera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        AssertDebug(entity->IsA<Camera>());

        return static_cast<Camera*>(entity);
    }

    return nullptr;
}

void Scene::SetWorld(World* world)
{
    HYP_SCOPE;
    AssertOnThread(m_ownerThreadId);

    if (m_world == world)
    {
        return;
    }

    if (m_world != nullptr && m_world->HasScene(Id()))
    {
        m_world->RemoveScene(this);
    }

    // When world is changed, entity manager needs all systems to have this change reflected
    m_entityManager->SetWorld(world);

    m_world = world;
}

Handle<Node> Scene::FindNodeByName(StringHash name) const
{
    HYP_SCOPE;
    AssertOnThread(m_ownerThreadId);

    Assert(m_root);

    if (m_root->GetName() == name)
    {
        return m_root;
    }

    return m_root->FindChildByName(name);
}

void Scene::SetRoot(const Handle<Node>& root)
{
    HYP_SCOPE;
    AssertOnThread(m_ownerThreadId);

    if (root == m_root)
    {
        return;
    }

    Handle<Node> prevRoot = m_root;

    if (prevRoot.IsValid() && prevRoot->GetScene() == this)
    {
        prevRoot->SetScene(nullptr);
    }

    m_root = root;

    if (m_root.IsValid())
    {
        m_root->SetScene(this);
    }

    OnRootNodeChanged(m_root, prevRoot);
}

bool Scene::AddToWorld(World* world)
{
    HYP_SCOPE;

    AssertOnThread(g_simThread);

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
    AssertOnThread(g_simThread);

    if (m_world == nullptr)
    {
        return false;
    }

    m_world->RemoveScene(this);

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

void Scene::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(m_ownerThreadId);

    if (m_sceneFlags & SceneFlags::HAS_OCTREE)
    {
        HYP_NAMED_SCOPE("Update octree");

        // Rebuild any octants that have had structural changes
        // IMPORTANT: must be ran at start of tick, as pointers to octants' visibility states will be
        // stored on VisibilityStateComponent.
        m_octree.PerformUpdates();
        m_octree.NextVisibilityState();
    }

    if (EntityManager* entityManager = m_entityManager)
    {
        entityManager->UpdateEntities(delta);
    }
}

#pragma endregion Scene

} // namespace Hyperion
