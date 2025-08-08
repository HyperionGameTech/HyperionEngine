/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <rendering/RenderStats.hpp>

#include <rendering/RenderBackend.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderCommand.hpp>

#include <core/object/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {

namespace sys {
class AppContextBase;
} // namespace sys

using sys::AppContextBase;

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

    HYP_FORCE_INLINE const Handle<AppContextBase>& GetAppContext() const
    {
        return m_appContext;
    }

    HYP_FORCE_INLINE void SetAppContext(const Handle<AppContextBase>& appContext)
    {
        m_appContext = appContext;
    }

    HYP_METHOD()
    const Handle<World>& GetCurrentWorld() const;
    
    HYP_METHOD()
    void SetCurrentWorld(const Handle<World>& world);
    
    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetDefaultWorld() const
    {
        return m_defaultWorld;
    }

    HYP_FORCE_INLINE DebugDrawer* GetDebugDrawer() const
    {
        return m_debugDrawer.Get();
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

    void FindTextureFormatDefaults();

    Handle<AppContextBase> m_appContext;

    UniquePtr<RenderThread> m_renderThread;

    Handle<World> m_world;

    UniquePtr<DebugDrawer> m_debugDrawer;

    UniquePtr<FinalPass> m_finalPass;

    UniquePtr<ScriptingService> m_scriptingService;
    
    FixedArray<Handle<World>, g_tripleBuffer ? 3 : 2> m_currentWorldBuffered;
    Handle<World> m_defaultWorld;

    EngineDelegates m_delegates;

    AtomicVar<bool> m_isShuttingDown;
    bool m_shouldRecreateSwapchain;
};

} // namespace hyperion

