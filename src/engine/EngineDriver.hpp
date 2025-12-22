/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/util/ShaderCompiler.hpp>

namespace hyperion {
namespace net {

class NetRequestThread;

} // namespace net

using net::NetRequestThread;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

class Game;
class SimThread;
class RenderGlobalState;
class ScriptingService;
class DebugDrawer;
class DeferredRenderer;
class FinalPass;
class PlaceholderData;
class RenderThread;
class SimThread;
class SafeDeleter;
class RenderState;
class World;
class EngineStats;

struct EngineDelegates
{
    Delegate<void> OnShutdown;
    Delegate<void> OnBeforeSwapchainRecreated;
    Delegate<void> OnAfterSwapchainRecreated;
};

HYP_CLASS()
class HYP_API EngineDriver final : public ObjectBase
{
    HYP_OBJECT_BODY(EngineDriver);

    friend struct RecreateSwapchain;
    friend class SimThread;
    friend class RenderThread;

public:
    HYP_METHOD()
    static const Handle<EngineDriver>& GetInstance();

    EngineDriver();
    ~EngineDriver() override;

    HYP_METHOD()
    World* GetCurrentWorld() const;

    HYP_METHOD()
    void SetCurrentWorld(World* world);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetDefaultWorld() const
    {
        return m_defaultWorld;
    }

    HYP_METHOD()
    void SetDefaultWorld(const Handle<World>& defaultWorld);

    HYP_FORCE_INLINE const Handle<DebugDrawer>& GetDebugDrawer() const
    {
        return m_debugDrawer;
    }

    HYP_FORCE_INLINE ScriptingService* GetScriptingService() const
    {
        return m_scriptingService.Get();
    }

    HYP_FORCE_INLINE EngineDelegates& GetDelegates()
    {
        return m_delegates;
    }

    HYP_FORCE_INLINE const EngineDelegates& GetDelegates() const
    {
        return m_delegates;
    }

    HYP_FORCE_INLINE bool IsShuttingDown() const
    {
        return AtomicAdd(&m_isShuttingDown, 0) > 0;
    }

    void AddWorld(const Handle<World>& world);
    void RemoveWorld(const World* world);

    bool IsRenderLoopActive() const;

    void StartThreads();

    void MainThreadUpdate();

    HYP_METHOD()
    void SetGameInstance(Game* gameInstance);

    HYP_METHOD()
    Game* GetGameInstance() const;

    void RequestStop();
    void FinalizeStop();

    Delegate<void, World*> OnCurrentWorldChanged;

private:
    void Init() override;

    void PreFrameUpdate(Frame* frame);

    void UpdateSim(float delta);

    /*! \brief Enqueue the given World for rendering on the render thread. */
    void EnqueueWorldRender(World* world);

    Handle<DebugDrawer> m_debugDrawer;

    UniquePtr<ScriptingService> m_scriptingService;

    Array<Handle<World>> m_worlds; // Game thread only
    World* m_currentWorld;         // Game thread only
    Handle<World> m_defaultWorld;

    Array<World*> m_worldsToRenderPerFrame[RingBufferDepth];

    EngineDelegates m_delegates;

    TaskBatch* m_viewCollectionBatch;

    mutable volatile int32 m_isShuttingDown;
};

} // namespace hyperion
