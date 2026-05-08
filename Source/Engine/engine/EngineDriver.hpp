/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/RenderObject.hpp>

#include <Core/Types.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/functional/Delegate.hpp>

#include <engine/config/EngineConfig.hpp>

#include <rendering/util/ShaderCompiler.hpp>

namespace Hyperion {
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
class DeletionQueue;
class RenderState;
class World;
class View;
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

    EngineDriver(const EngineDriver&) = delete;
    EngineDriver& operator=(const EngineDriver&) = delete;

    ~EngineDriver() override;

    HYP_METHOD()
    World* GetCurrentWorld() const;

    HYP_METHOD()
    void SetCurrentWorld(World* world);

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

    Span<View* const> GetCurrentFrameViews() const;

    bool IsRenderLoopActive() const;

    bool StartThreads();

    HYP_METHOD()
    void SetGameInstance(Game* gameInstance);

    HYP_METHOD()
    Game* GetGameInstance() const;

    EngineConfig& GetConfig();

    void Initialize();

    void RequestStop();
    void FinalizeStop();

    Delegate<void, World*> OnCurrentWorldChanged;

private:
    void SyncConfig();

    void UpdateSim(float delta);

    Handle<DebugDrawer> m_debugDrawer;

    UniquePtr<ScriptingService> m_scriptingService;

    Array<Handle<World>> m_worlds; // Sim thread only
    World* m_currentWorld;         // Sim thread only

    Array<View*> m_viewsPerFrame[RingBufferDepth];

    EngineDelegates m_delegates;

    TaskBatch* m_viewCollectionBatch;

    bool m_isInitialized;
    mutable volatile int32 m_isShuttingDown;
};

} // namespace Hyperion
