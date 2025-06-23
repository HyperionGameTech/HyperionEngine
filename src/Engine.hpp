/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_HPP
#define HYPERION_ENGINE_HPP

#include <Config.hpp>
#include <Types.hpp>
#include <EngineGlobals.hpp>

#include <rendering/EngineRenderStats.hpp>

#include <rendering/backend/RenderingAPI.hpp>

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/Handle.hpp>
#include <core/Base.hpp>

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
class RenderView;
class FinalPass;
class PlaceholderData;
class RenderThread;
class SafeDeleter;
class RenderState;
class MaterialDescriptorSetManager;
class GraphicsPipelineCache;

struct EngineDelegates
{
    Delegate<void> OnShutdown;
    Delegate<void> OnBeforeSwapchainRecreated;
    Delegate<void> OnAfterSwapchainRecreated;
};

HYP_CLASS()
class Engine final : public HypObject<Engine>
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
        return m_app_context;
    }

    HYP_FORCE_INLINE void SetAppContext(const Handle<AppContextBase>& app_context)
    {
        m_app_context = app_context;
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

    HYP_FORCE_INLINE ShaderCompiler& GetShaderCompiler()
    {
        return m_shader_compiler;
    }

    HYP_FORCE_INLINE const ShaderCompiler& GetShaderCompiler() const
    {
        return m_shader_compiler;
    }

    HYP_FORCE_INLINE DebugDrawer* GetDebugDrawer() const
    {
        return m_debug_drawer.Get();
    }

    HYP_FORCE_INLINE FinalPass* GetFinalPass() const
    {
        return m_final_pass.Get();
    }

    HYP_FORCE_INLINE ScriptingService* GetScriptingService() const
    {
        return m_scripting_service.Get();
    }

    HYP_FORCE_INLINE MaterialDescriptorSetManager* GetMaterialDescriptorSetManager()
    {
        return m_material_descriptor_set_manager.Get();
    }

    HYP_FORCE_INLINE GraphicsPipelineCache* GetGraphicsPipelineCache() const
    {
        return m_graphics_pipeline_cache.Get();
    }

    HYP_FORCE_INLINE EngineDelegates& GetDelegates()
    {
        return m_delegates;
    }

    HYP_FORCE_INLINE const EngineDelegates& GetDelegates() const
    {
        return m_delegates;
    }

    HYP_FORCE_INLINE EngineRenderStatsCalculator& GetRenderStatsCalculator()
    {
        return m_render_stats_calculator;
    }

    HYP_FORCE_INLINE bool IsShuttingDown() const
    {
        return m_is_shutting_down.Get(MemoryOrder::SEQUENTIAL);
    }

    HYP_API bool IsRenderLoopActive() const;
    HYP_API bool StartRenderLoop();

    HYP_API void RenderNextFrame();
    HYP_API void RequestStop();
    
    void FinalizeStop();
    
    Delegate<void, EngineRenderStats> OnRenderStatsUpdated;
    
private:
    HYP_API void Init() override;

    void PreFrameUpdate(FrameBase* frame);

    void FindTextureFormatDefaults();

    Handle<AppContextBase> m_app_context;

    UniquePtr<RenderThread> m_render_thread;

    ShaderCompiler m_shader_compiler;

    UniquePtr<MaterialDescriptorSetManager> m_material_descriptor_set_manager;

    Handle<World> m_world;

    Configuration m_configuration;

    UniquePtr<DebugDrawer> m_debug_drawer;

    UniquePtr<FinalPass> m_final_pass;

    UniquePtr<ScriptingService> m_scripting_service;

    UniquePtr<GraphicsPipelineCache> m_graphics_pipeline_cache;

    EngineDelegates m_delegates;

    EngineRenderStatsCalculator m_render_stats_calculator;
    EngineRenderStats m_render_stats;

    AtomicVar<bool> m_is_shutting_down;
    bool m_should_recreate_swapchain;
};

} // namespace hyperion

#endif
