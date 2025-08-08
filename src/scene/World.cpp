/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/World.hpp>
#include <scene/View.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorSubsystem.hpp>
#endif

#include <scene/EntityManager.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/config/Config.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderProxy.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_API extern const GlobalConfig& GetGlobalConfig();

#define HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
#define HYP_WORLD_ASYNC_VIEW_COLLECTION

World::World()
    : HypObjectBase(),
      m_name(Name::Unique("World")),
      m_worldGrid(CreateObject<WorldGrid>(this)),
      m_detachedScenes(this),
      m_raytracingView(nullptr),
      m_viewCollectionBatch(nullptr)
{
    // set m_viewsPerFrame to initial size. It uses fixed allocator so it won't dynamically allocate any memory anyway
    m_viewsPerFrame.Resize(m_viewsPerFrame.Capacity());
    AssertDebug(m_viewsPerFrame.Size() == (g_tripleBuffer ? 3 : 2));
}

World::~World()
{
    if (IsReady())
    {
        for (const Handle<Scene>& scene : m_scenes)
        {
            if (!scene.IsValid())
            {
                continue;
            }

            OnSceneRemoved(this, scene);

            for (auto& it : m_subsystems)
            {
                it.second->OnSceneDetached(scene);
            }

            if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
            {
                for (const Handle<View>& view : m_views)
                {
                    if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                    {
                        continue;
                    }

                    view->RemoveScene(scene);
                }
            }

            scene->SetWorld(nullptr);
        }

        m_raytracingView = nullptr;

        m_scenes.Clear();
        m_views.Clear();

        for (auto& it : m_subsystems)
        {
            const Handle<Subsystem>& subsystem = it.second;
            Assert(subsystem.IsValid());

            it.second->OnRemovedFromWorld();
        }

        if (m_viewCollectionBatch)
        {
            AssertDebug(m_viewCollectionBatch->IsCompleted());

            delete m_viewCollectionBatch;
            m_viewCollectionBatch = nullptr;
        }
    }

    m_physicsWorld.Teardown();

    if (m_worldGrid.IsValid())
    {
        m_worldGrid->Shutdown();
    }
}

void World::Init()
{
    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]
        {
            if (m_worldGrid.IsValid())
            {
                m_worldGrid->Shutdown();
            }
        }));

    m_viewCollectionBatch = new TaskBatch();
    m_viewCollectionBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_GENERIC);

    for (auto& it : m_subsystems)
    {
        const Handle<Subsystem>& subsystem = it.second;
        Assert(subsystem.IsValid());

        InitObject(subsystem);

        subsystem->OnAddedToWorld();
    }

    InitObject(m_worldGrid);

    // Create a View that is intended to collect objects used by RT gi/reflections
    // since we'll need to have resources bound even if they aren't directly in any camera's view frustum.
    // (for example there could be some stuff behind the player we want to see reflections of)
    if (GetGlobalConfig().Get("rendering.raytracing.enabled").ToBool(false))
    {
        // dummy output target
        ViewOutputTargetDesc outputTargetDesc {
            .extent = Vec2u::One(),
            .attachments = { { TF_R8 } }
        };

        const ViewDesc raytracingViewDesc {
            .flags = ViewFlags::RAYTRACING | ViewFlags::NO_DRAW_CALLS
                | ViewFlags::ALL_WORLD_SCENES | ViewFlags::COLLECT_ALL_ENTITIES
                | ViewFlags::NO_FRUSTUM_CULLING,
            .viewport = Viewport { .extent = Vec2u::One(), .position = Vec2i::Zero() },
            .outputTargetDesc = outputTargetDesc,
            .camera = CreateObject<Camera>()
        };

        Handle<View> raytracingView = CreateObject<View>(raytracingViewDesc);
        InitObject(raytracingView);

        m_raytracingView = raytracingView;

        m_views.PushBack(std::move(raytracingView));
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->SetWorld(this);

        InitObject(scene);

        OnSceneAdded(this, scene);

        for (auto& it : m_subsystems)
        {
            it.second->OnSceneAttached(scene);
        }

        if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (const Handle<View>& view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }
    }

    for (const Handle<View>& view : m_views)
    {
        if (view->m_raytracingView.GetUnsafe() != m_raytracingView)
        {
            if (view->m_raytracingView.IsValid())
            {
                HYP_LOG(Scene, Warning,
                    "View {} already has a raytracing View set! Was it added to multiple Worlds with raytracing enabled?",
                    view->Id());

                view->m_raytracingView.Reset();
            }

            if (m_raytracingView != nullptr)
            {
                view->m_raytracingView = m_raytracingView->WeakHandleFromThis();
            }
        }

        InitObject(view);
    }

    m_physicsWorld.Init();

    SetReady(true);
}

void World::ProcessViewAsync(const Handle<View>& view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (!view)
    {
        return;
    }

    if (m_processViews.Contains(view))
    {
        return;
    }

    m_processViews.PushBack(view);
}

DelegateHandler World::ProcessViewAsync(const Handle<View>& view, Proc<void()>&& onComplete)
{
    if (!onComplete.IsValid())
    {
        ProcessViewAsync(view);

        return {};
    }

    ProcessViewAsync(view);

    return m_viewCollectionBatch->OnComplete.Bind(std::move(onComplete));
}

void World::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    AssertReady();

    const uint32 currentFrameIndex = RenderApi_GetFrameIndex();

    // set buffered Views for current frame index
    m_viewsPerFrame[currentFrameIndex] = m_views;

    Array<View*> processViews;
    processViews.Resize(m_processViews.Size() + m_views.Size());

    for (SizeType i = 0; i < m_views.Size(); i++)
    {
        AssertDebug(m_views[i].IsValid());
        AssertDebug(!m_processViews.Contains(m_views[i]));

        processViews[i] = m_views[i].Get();
    }

    for (SizeType i = m_views.Size(); i < processViews.Size(); i++)
    {
        processViews[i] = m_processViews[i - m_views.Size()];
    }

    // Clear additional Views to process for next frame
    m_processViews.Clear();

    m_gameState.deltaTime = delta;

    m_worldGrid->Update(delta);

    m_physicsWorld.Tick(delta);

#ifdef HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
    Array<Task<void>> updateSubsystemTasks;

    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        if (subsystem->RequiresUpdateOnGameThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);

        updateSubsystemTasks.PushBack(TaskSystem::GetInstance().Enqueue([subsystem, delta]
            {
                HYP_NAMED_SCOPE_FMT("Update subsystem: {}", subsystem->InstanceClass()->GetName());

                subsystem->Update(delta);
            }));
    }

    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        if (!subsystem->RequiresUpdateOnGameThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }

    for (Task<void>& task : updateSubsystemTasks)
    {
        task.Await();
    }

    updateSubsystemTasks.Clear();
#else
    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }
#endif

    Array<EntityManager*> entityManagers;
    entityManagers.Reserve(m_scenes.Size());

    for (uint32 index = 0; index < m_scenes.Size(); index++)
    {
        const Handle<Scene>& scene = m_scenes[index];

        if (!scene.IsValid())
        {
            continue;
        }

        // sanity checks
        Assert(scene->GetWorld() == this);
        Assert(!(scene->GetFlags() & SceneFlags::DETACHED));

        scene->Update(delta);

        entityManagers.PushBack(scene->GetEntityManager().Get());
    }

#ifdef HYP_WORLD_ASYNC_VIEW_COLLECTION
    AssertDebug(m_viewCollectionBatch != nullptr);
    AssertDebug(m_viewCollectionBatch->IsCompleted());

    m_viewCollectionBatch->ResetState();
#endif

    for (EntityManager* entityManager : entityManagers)
    {
        HYP_NAMED_SCOPE("Call BeginAsyncUpdate on EntityManager");

        AssertDebug(entityManager->GetWorld() == this);

        entityManager->BeginAsyncUpdate(delta);
    }

    for (EntityManager* entityManager : entityManagers)
    {
        HYP_NAMED_SCOPE("Call EndAsyncUpdate on EntityManager");

        entityManager->EndAsyncUpdate();
    }

    for (uint32 index = 0; index < processViews.Size(); index++)
    {
        HYP_NAMED_SCOPE("Per-view entity collection");

        View* view = processViews[index];
        Assert(view != nullptr);

        view->UpdateViewport();
        // View must be updated on the game thread as it mutates the scene's octree state
        view->UpdateVisibility();

#ifdef HYP_WORLD_ASYNC_VIEW_COLLECTION
        view->BeginAsyncCollection(*m_viewCollectionBatch);
#else
        view->CollectSync();
#endif
    }

#ifdef HYP_WORLD_ASYNC_VIEW_COLLECTION
    TaskSystem::GetInstance().EnqueueBatch(m_viewCollectionBatch);
    m_viewCollectionBatch->AwaitCompletion();

    for (uint32 index = 0; index < processViews.Size(); index++)
    {
        processViews[index]->EndAsyncCollection();
    }
#endif

    m_gameState.gameTime += delta;

    WorldShaderData* bufferData = RenderApi_GetWorldBufferData();
    bufferData->gameTime = m_gameState.gameTime;
    bufferData->frameCounter = RenderApi_GetFrameCounter();
}

Handle<Subsystem> World::AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem)
{
    HYP_SCOPE;

    if (!subsystem)
    {
        return nullptr;
    }

    Threads::AssertOnThread(g_gameThread);

    subsystem->SetWorld(this);

    const auto it = m_subsystems.Find(typeId);
    Assert(it == m_subsystems.End(), "Subsystem of type %s already exists in World", *subsystem->InstanceClass()->GetName());

    auto insertResult = m_subsystems.Set(typeId, subsystem);

    // Create a new Handle, calling OnAddedToWorld() may add new subsystems which would invalidate the iterator
    Handle<Subsystem> newSubsystem = insertResult.first->second;
    Assert(newSubsystem.IsValid());

    // If World is already initialized, initialize the subsystem
    // otherwise, it will be initialized when World::Init() is called
    if (IsReady())
    {
        if (insertResult.second)
        {
            InitObject(newSubsystem);

            newSubsystem->OnAddedToWorld();

            for (Handle<Scene>& scene : m_scenes)
            {
                newSubsystem->OnSceneAttached(scene);
            }
        }
    }

    return newSubsystem;
}

Subsystem* World::GetSubsystem(TypeId typeId) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.Find(typeId);

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

Subsystem* World::GetSubsystemByName(WeakName name) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.FindIf([name](const Pair<TypeId, Handle<Subsystem>>& item)
        {
            if (!item.second)
            {
                return false;
            }

            const HypClass* hypClass = item.second->InstanceClass();

            return hypClass->GetName() == name;
        });

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

bool World::RemoveSubsystem(Subsystem* subsystem)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!subsystem)
    {
        return false;
    }

    const TypeId typeId = subsystem->InstanceClass()->GetTypeId();

    auto it = m_subsystems.Find(typeId);

    if (it == m_subsystems.End())
    {
        return false;
    }

    Assert(it->second.Get() == subsystem);

    if (IsReady())
    {
        for (const Handle<Scene>& scene : m_scenes)
        {
            if (!scene.IsValid())
            {
                continue;
            }

            subsystem->OnSceneDetached(scene);
        }

        subsystem->OnRemovedFromWorld();
    }

    subsystem->SetWorld(nullptr);

    m_subsystems.Erase(it);

    return true;
}

void World::StartSimulating()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (m_gameState.mode == GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::SIMULATING;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::SIMULATING);
}

void World::StopSimulating()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (m_gameState.mode != GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::EDITOR;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::EDITOR);
}

void World::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        HYP_LOG(Scene, Warning, "Scene {} already exists in world", scene->GetName());

        return;
    }

    scene->SetWorld(this);

    if (IsReady())
    {
        InitObject(scene);

        OnSceneAdded(this, scene);

        for (auto& it : m_subsystems)
        {
            it.second->OnSceneAttached(scene);
        }

        if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (const Handle<View>& view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }
    }

    m_scenes.PushBack(scene);
}

bool World::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    // @TODO RemoveScene needs to trigger a scene detach event for all rendersubsystems

    typename Array<Handle<Scene>>::Iterator it = m_scenes.Find(scene);

    if (it == m_scenes.End())
    {
        return false;
    }

    Handle<Scene> sceneCopy = *it;

    m_scenes.Erase(it);

    if (scene.IsValid())
    {
        OnSceneRemoved(this, scene);

        scene->SetWorld(nullptr);

        if (IsReady())
        {
            for (auto& it : m_subsystems)
            {
                it.second->OnSceneDetached(scene);
            }

            for (const Handle<View>& view : m_views)
            {
                view->RemoveScene(scene);
            }
        }
    }

    return true;
}

bool World::HasScene(ObjId<Scene> sceneId) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    return m_scenes.FindIf([sceneId](const Handle<Scene>& scene)
               {
                   return scene.Id() == sceneId;
               })
        != m_scenes.End();
}

const Handle<Scene>& World::GetSceneByName(Name name) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    const auto it = m_scenes.FindIf([name](const Handle<Scene>& scene)
        {
            return scene->GetName() == name;
        });

    return it != m_scenes.End() ? *it : Handle<Scene>::empty;
}

const Handle<Scene>& World::GetDetachedScene(const ThreadId& threadId)
{
    return m_detachedScenes.GetDetachedScene(threadId);
}

void World::AddView(const Handle<View>& view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!view.IsValid())
    {
        return;
    }

    m_views.PushBack(view);

    if (IsReady())
    {
        if (view->m_raytracingView.GetUnsafe() != m_raytracingView)
        {
            if (view->m_raytracingView.IsValid())
            {
                HYP_LOG(Scene, Warning,
                    "View {} already has a raytracing View set! Was it added to multiple Worlds with raytracing enabled?",
                    view->Id());

                view->m_raytracingView.Reset();
            }

            if (m_raytracingView != nullptr)
            {
                view->m_raytracingView = m_raytracingView->WeakHandleFromThis();
            }
        }

        // Add all scenes to the view, if the view should collect all world scenes
        if (view->GetFlags() & ViewFlags::ALL_WORLD_SCENES)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (!scene.IsValid())
                {
                    continue;
                }

                if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
                {
                    view->AddScene(scene);
                }
            }
        }

        InitObject(view);
    }
}

void World::RemoveView(const Handle<View>& view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!view.IsValid())
    {
        return;
    }

    typename Array<Handle<View>>::Iterator it = m_views.Find(view);

    if (IsReady())
    {
        view->m_raytracingView.Reset();

        // Remove all scenes from the view, if the view should collect all world scenes
        if (view->GetFlags() & ViewFlags::ALL_WORLD_SCENES)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (!scene.IsValid())
                {
                    continue;
                }

                view->RemoveScene(scene);
            }
        }
    }

    if (it != m_views.End())
    {
        m_views.Erase(it);
    }
}

Span<const Handle<View>> World::GetViews() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread | g_gameThread);

    return m_viewsPerFrame[RenderApi_GetFrameIndex()].ToSpan();
}

RenderStats* World::GetRenderStats() const
{
    return RenderApi_GetRenderStats();
}

} // namespace hyperion
