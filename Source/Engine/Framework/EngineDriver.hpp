/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Types.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/AtomicFlag.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Framework/Config/EngineConfig.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>

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
class DebugDrawer;
class DeferredPass;
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
class ENGINE_API EngineDriver final : public ObjectBase
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
        return m_isShuttingDown.LoadVolatile();
    }

    void AddWorld(const Handle<World>& world);
    void RemoveWorld(const World* world);

    Span<View* const> GetCurrentFrameViews() const;

    bool StartThreads();

    HYP_METHOD()
    void SetGameInstance(Game* gameInstance);

    HYP_METHOD()
    Game* GetGameInstance() const;

    void Initialize();
    void Shutdown();

    Delegate<void, World*> OnCurrentWorldChanged;

private:
    void Simulate(float delta, Game* gameInstance);

    Array<Handle<World>> m_worlds; // Sim thread only
    World* m_currentWorld;         // Sim thread only

    Array<View*> m_viewsPerFrame[RingBufferDepth];

    EngineDelegates m_delegates;

    TaskBatch* m_viewCollectionBatch;

    bool m_isInitialized;

    AtomicFlag m_isShuttingDown;
};

} // namespace Hyperion
