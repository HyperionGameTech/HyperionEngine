/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Light.hpp>

#include <Scene/EntityManager.hpp>

#include <Rendering/RenderInterface.hpp>

#include <Rendering/AccelerationStructure.hpp>

#include <System/AppContext.hpp>

#include <Core/Math/Halton.hpp>

#include <Framework/EngineGlobals.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG

#include <Scene.generated.inl>

namespace Hyperion {

ScriptableDelegate<void, Handle<Node>, Handle<Node>> Scene::OnRootNodeChanged;


static const Name s_nameUnnamedScene = NAME("<unnamed scene>");
static const Name s_nameSceneRoot = NAME("<ROOT>");

void Scene_OnPostLoad(Scene& scene)
{
    if (!EngineGlobals::IsShuttingDown())
    {
        scene.SetOwnerThreadId(g_simThread);
    }
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
      m_previousDelta(0.01667f),
      m_isInitialized(false)
{
    m_root = MakeHandle<Node>(s_nameSceneRoot, Transform::identity, this);
    m_root->SetIsStatic(false);
}

Scene::~Scene()
{
    Shutdown();

    // We need to ensure root gets destroyed before the EntityManager does.
    // Otherwise we'll get some issues in ~Entity() trying to remove self from the emgr
    m_root.Reset();
    m_entityManager.Reset();

    OnRootNodeChanged.RemoveAllForTarget(this);
}

void Scene::Initialize()
{
    if (m_isInitialized)
    {
        return;
    }

    m_isInitialized = true;

    if (!m_root.IsValid())
    {
        m_root = MakeHandle<Node>(s_nameSceneRoot, Transform::identity, this);
        m_root->SetIsStatic(false);
    }

    if (!m_entityManager.IsValid())
    {
        m_entityManager = MakeHandle<EntityManager>(m_ownerThreadId, this);
    }

    m_entityManager->SetWorld(m_world);

    m_octree.SetEntityManager(m_entityManager);

    InitObject(m_root);

    m_entityManager->Initialize();

    AssertDebug(m_entityManager->GetWorld() == m_world);
}

void Scene::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

    m_isInitialized = false;

    m_octree.SetEntityManager(nullptr);
    m_octree.Clear();

    // Move so destruction of components can check GetEntityManager() returns nullptr
    if (m_entityManager.IsValid())
    {
        if (IsOnThread(m_entityManager->GetOwnerThreadId()))
        {
            // If we are on the same thread, we can safely shutdown the entity manager here:
            m_entityManager->Shutdown();

            m_entityManager.Reset();
        }
        else
        {
            // have to enqueue a task to shut down the entity manager on its owner thread
            Task<void> task = GetThreadById(m_entityManager->GetOwnerThreadId())->GetScheduler().Enqueue([entityManager = std::move(m_entityManager)]()
                {
                    entityManager->Shutdown();
                });

            // Wait for shut down to complete
            task.Await();
        }
    }

    m_root.Reset();

    m_world = nullptr;
}

void Scene::SetOwnerThreadId(ThreadId ownerThreadId)
{
    if (m_ownerThreadId == ownerThreadId)
    {
        return;
    }

    m_ownerThreadId = ownerThreadId;

    if (m_entityManager.IsValid())
    {
        m_entityManager->SetOwnerThreadId(ownerThreadId);
    }
}

Camera* Scene::GetPrimaryCamera() const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!m_entityManager.IsValid())
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
    AssertOnThread(m_ownerThreadId);

    if (m_world == world)
    {
        return;
    }

    if (m_world != nullptr && m_world->HasScene(Id()))
    {
        m_world->RemoveScene(this);
    }

    m_world = world;

    if (m_entityManager.IsValid())
    {
        // When world is changed, entity manager needs all systems to have this change reflected
        m_entityManager->SetWorld(world);
    }
}

Handle<Node> Scene::FindNodeByName(StringHash name) const
{
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

    OnRootNodeChanged.Fire(this, m_root, prevRoot);
}

bool Scene::AddToWorld(World* world)
{
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
