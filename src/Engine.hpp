/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_HPP
#define HYPERION_ENGINE_HPP

#include <Config.hpp>

#include <rendering/DefaultFormats.hpp>

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/Handle.hpp>
#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/CrashHandler.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include <Types.hpp>

namespace hyperion {

using renderer::Instance;

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

namespace net {
class NetRequestThread;
} // namespace net

class Engine;
class Game;
class GameThread;
class ShaderGlobals;
class ScriptingService;
class AssetManager;
class DebugDrawer;
class DeferredRenderer;
class FinalPass;
class PlaceholderData;
class RenderThread;
class SafeDeleter;
class ShaderManager;
class RenderState;
class MaterialCache;
class MaterialDescriptorSetManager;

extern Handle<Engine> g_engine;
extern Handle<AssetManager> g_asset_manager;
extern ShaderManager *g_shader_manager;
extern MaterialCache *g_material_system;
extern SafeDeleter *g_safe_deleter;

struct DebugMarker
{
    const CommandBuffer *command_buffer;
    const char * const  name = "<Unnamed debug marker>";
    bool                is_ended = false;

    DebugMarker(const CommandBuffer *command_buffer, const char *marker_name)
        : command_buffer(command_buffer),
          name(marker_name)
    {
        if (command_buffer) {
            command_buffer->DebugMarkerBegin(name);
        }
    }

    DebugMarker(const DebugMarker &other) = delete;
    DebugMarker &operator=(const DebugMarker &other) = delete;

    DebugMarker(DebugMarker &&other) noexcept = delete;
    DebugMarker &operator=(DebugMarker &&other) noexcept = delete;

    ~DebugMarker()
    {
        MarkEnd();
    }

    void MarkEnd()
    {
        if (is_ended) {
            return;
        }

        if (command_buffer != nullptr) {
            command_buffer->DebugMarkerEnd();
        }

        is_ended = true;
    }
};

class GlobalDescriptorSetManager
{
public:
    GlobalDescriptorSetManager(Engine *engine);
    GlobalDescriptorSetManager(const GlobalDescriptorSetManager &)                  = delete;
    GlobalDescriptorSetManager &operator=(const GlobalDescriptorSetManager &)       = delete;
    GlobalDescriptorSetManager(GlobalDescriptorSetManager &&) noexcept              = delete;
    GlobalDescriptorSetManager &operator=(GlobalDescriptorSetManager &&) noexcept   = delete;
    ~GlobalDescriptorSetManager();

    void Initialize(Engine *engine);

    void AddDescriptorSet(Name name, const DescriptorSetRef &ref);
    DescriptorSetRef GetDescriptorSet(Name name) const;

private:
    HashMap<Name, DescriptorSetRef> m_descriptor_sets;
    mutable Mutex                   m_mutex;
};

class EntityInstanceBatchHolderMap;

struct EngineDelegates
{
    Delegate<void>  OnShutdown;
    Delegate<void>  OnBeforeSwapchainRecreated;
    Delegate<void>  OnAfterSwapchainRecreated;
};

HYP_CLASS()
class Engine : public HypObject<Engine>
{
    HYP_OBJECT_BODY(Engine);

public:
    HYP_API static const Handle<Engine> &GetInstance();

    HYP_API Engine();
    HYP_API ~Engine();

    HYP_FORCE_INLINE const RC<AppContext> &GetAppContext() const
        { return m_app_context; }

    HYP_FORCE_INLINE Instance *GetGPUInstance() const
        { return m_instance.Get(); }

    HYP_FORCE_INLINE Device *GetGPUDevice() const
        { return m_instance ? m_instance->GetDevice() : nullptr; }
    
    HYP_FORCE_INLINE DeferredRenderer *GetDeferredRenderer() const
        { return m_deferred_renderer.Get(); }
    
    HYP_FORCE_INLINE const Handle<RenderState> &GetRenderState() const
        { return m_render_state; }
    
    HYP_FORCE_INLINE ShaderGlobals *GetRenderData() const
        { return m_render_data.Get(); }
    
    HYP_FORCE_INLINE PlaceholderData *GetPlaceholderData() const
        { return m_placeholder_data.Get(); }
    
    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World> &GetWorld() const
        { return m_world; }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World> &GetDefaultWorld() const
        { return m_world; }
    
    HYP_FORCE_INLINE HYP_DEPRECATED Configuration &GetConfig()
        { return m_configuration; }
    
    HYP_FORCE_INLINE HYP_DEPRECATED const Configuration &GetConfig() const
        { return m_configuration; }

    HYP_FORCE_INLINE ShaderCompiler &GetShaderCompiler()
        { return m_shader_compiler; }
    
    HYP_FORCE_INLINE  const ShaderCompiler &GetShaderCompiler() const
        { return m_shader_compiler; }
    
    HYP_FORCE_INLINE DebugDrawer *GetDebugDrawer() const
        { return m_debug_drawer.Get(); }
    
    HYP_FORCE_INLINE InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.At(type); }
    
    HYP_FORCE_INLINE FinalPass *GetFinalPass() const
        { return m_final_pass.Get(); }

    HYP_FORCE_INLINE ScriptingService *GetScriptingService() const
        { return m_scripting_service.Get(); }
    
    HYP_FORCE_INLINE const DescriptorTableRef &GetGlobalDescriptorTable() const
        { return m_global_descriptor_table; }
    
    HYP_FORCE_INLINE MaterialDescriptorSetManager *GetMaterialDescriptorSetManager()
        { return m_material_descriptor_set_manager.Get(); }

    HYP_FORCE_INLINE net::NetRequestThread *GetNetRequestThread() const
        { return m_net_request_thread.Get(); }

    HYP_FORCE_INLINE EntityInstanceBatchHolderMap *GetEntityInstanceBatchHolderMap() const
        { return m_entity_instance_batch_holder_map.Get(); }

    HYP_FORCE_INLINE EngineDelegates &GetDelegates()
        { return m_delegates; }

    HYP_FORCE_INLINE const EngineDelegates &GetDelegates() const
        { return m_delegates; }

    HYP_FORCE_INLINE bool IsShuttingDown() const
        { return m_is_shutting_down.Get(MemoryOrder::SEQUENTIAL); }

    bool IsRenderLoopActive() const;

    HYP_API void Initialize(const RC<AppContext> &app_context);

    HYP_API void RenderNextFrame(Game *game);
    HYP_API void RequestStop();

    ShaderCompiler m_shader_compiler;

    AtomicVar<bool> m_stop_requested;

    void FinalizeStop();

private:
    void UpdateBuffersAndDescriptors(uint32 frame_index);

    void PreFrameUpdate(Frame *frame);
    void RenderDeferred(Frame *frame);

    void FindTextureFormatDefaults();

    RC<AppContext>                                          m_app_context;

    UniquePtr<RenderThread>                                 m_render_thread;
    
    UniquePtr<Instance>                                     m_instance;

    UniquePtr<PlaceholderData>                              m_placeholder_data;

    DescriptorTableRef                                      m_global_descriptor_table;

    UniquePtr<MaterialDescriptorSetManager>                 m_material_descriptor_set_manager;

    HashMap<TextureFormatDefault, InternalFormat>           m_texture_format_defaults;

    UniquePtr<DeferredRenderer>                             m_deferred_renderer;

    UniquePtr<ShaderGlobals>                                m_render_data;

    Handle<World>                                           m_world;
    
    Configuration                                           m_configuration;

    UniquePtr<DebugDrawer>                                  m_debug_drawer;

    UniquePtr<FinalPass>                                    m_final_pass;

    UniquePtr<ScriptingService>                             m_scripting_service;

    RC<net::NetRequestThread>                               m_net_request_thread;

    UniquePtr<EntityInstanceBatchHolderMap>                 m_entity_instance_batch_holder_map;

    Handle<RenderState>                                     m_render_state;

    CrashHandler                                            m_crash_handler;

    EngineDelegates                                         m_delegates;

    AtomicVar<bool>                                         m_is_shutting_down { false };
    bool                                                    m_is_initialized;
};

} // namespace hyperion

#endif

