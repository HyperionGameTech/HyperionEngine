/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Config.hpp>
#include <Types.hpp>

#include <rendering/RenderStats.hpp>

#include <rendering/RenderBackend.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderCommand.hpp>

#include <core/Handle.hpp>

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
class Engine final : public HypObjectBase
{
    HYP_OBJECT_BODY(Engine);

    friend struct RENDER_COMMAND(RecreateSwapchain);

public:
    HYP_METHOD()
    HYP_API static const Handle<Engine>& GetInstance();

    HYP_API Engine();
    HYP_API ~Engine() override;

    HYP_FORCE_INLINE const Handle<AppContextBase>& GetAppContext() const
    {
        return m_appContext;
    }

    HYP_FORCE_INLINE void SetAppContext(const Handle<AppContextBase>& appContext)
    {
        m_appContext = appContext;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetDefaultWorld() const
    {
        return m_world;
    }

    HYP_FORCE_INLINE HYP_DEPRECATED Configuration& GetConfig()
    {
        return m_configuration;
    }

    HYP_FORCE_INLINE HYP_DEPRECATED const Configuration& GetConfig() const
    {
        return m_configuration;
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

    Configuration m_configuration;

    UniquePtr<DebugDrawer> m_debugDrawer;

    UniquePtr<FinalPass> m_finalPass;

    UniquePtr<ScriptingService> m_scriptingService;

    EngineDelegates m_delegates;

    AtomicVar<bool> m_isShuttingDown;
    bool m_shouldRecreateSwapchain;
};

} // namespace hyperion

