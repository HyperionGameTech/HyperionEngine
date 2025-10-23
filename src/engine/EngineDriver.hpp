/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderStats.hpp>

#include <rendering/RenderBackend.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderCommand.hpp>

#include <core/Types.hpp>

#include <core/reflection/Handle.hpp>

#include <core/reflection/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {
namespace net {

class NetRequestThread;

} // namespace net

using net::NetRequestThread;

class Game;
class GameThread;
class RenderGlobalState;
class ScriptingService;
class DebugDrawer;
class DeferredRenderer;
class FinalPass;
class PlaceholderData;
class RenderThread;
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
class EngineDriver final : public HypObjectBase
{
    HYP_OBJECT_BODY(EngineDriver);

    friend struct RENDER_COMMAND(RecreateSwapchain);

public:
    HYP_METHOD()
    HYP_API static const Handle<EngineDriver>& GetInstance();

    HYP_API EngineDriver();
    HYP_API ~EngineDriver() override;

    /*! \brief Callable on the render thread or game thread */
    HYP_METHOD()
    const Handle<EngineStats>& GetEngineStats() const;

    HYP_METHOD()
    const Handle<World>& GetCurrentWorld() const;

    HYP_METHOD()
    void SetCurrentWorld(const Handle<World>& world);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetDefaultWorld() const
    {
        return m_defaultWorld;
    }

    HYP_FORCE_INLINE const Handle<DebugDrawer>& GetDebugDrawer() const
    {
        return m_debugDrawer;
    }

    HYP_FORCE_INLINE FinalPass* GetFinalPass() const
    {
        return m_finalPass.Get();
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
        return m_isShuttingDown.Get(MemoryOrder::SEQUENTIAL);
    }

    HYP_API bool IsRenderLoopActive() const;
    HYP_API bool StartRenderLoop();

    HYP_API void RenderNextFrame();
    HYP_API void RequestStop();

    void FinalizeStop();

private:
    HYP_API void Init() override;

    void PreFrameUpdate(FrameBase* frame);

    FixedArray<Handle<EngineStats>, NumMultiBuffers> m_engineStatsBuffered;

    UniquePtr<RenderThread> m_renderThread;

    Handle<World> m_world;

    Handle<DebugDrawer> m_debugDrawer;

    UniquePtr<FinalPass> m_finalPass;

    UniquePtr<ScriptingService> m_scriptingService;

    FixedArray<Handle<World>, NumMultiBuffers> m_currentWorldBuffered;
    Handle<World> m_defaultWorld;

    EngineDelegates m_delegates;

    AtomicVar<bool> m_isShuttingDown;
    bool m_shouldRecreateSwapchain;
};

} // namespace hyperion
