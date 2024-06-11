/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_HPP
#define HYPERION_ENGINE_HPP

#include <Config.hpp>

#include <asset/Assets.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/DefaultFormats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/Material.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <scene/World.hpp>

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/functional/Delegate.hpp>
#include <core/ObjectPool.hpp>

#include <rendering/CrashHandler.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include <Types.hpp>

#include <mutex>

namespace hyperion {

using renderer::Instance;

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

class Engine;
class Game;
class GameThread;
class ShaderGlobals;
class ScriptingService;

extern Engine               *g_engine;
extern AssetManager         *g_asset_manager;
extern ShaderManagerSystem  *g_shader_manager;
extern MaterialCache        *g_material_system;
extern SafeDeleter          *g_safe_deleter;

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
    HashMap<Name, DescriptorSetRef>    m_descriptor_sets;
    mutable Mutex                       m_mutex;
};

struct EngineDelegates
{
    Delegate<void>  OnShutdown;
};

class Engine
{
#ifdef HYP_DEBUG_MODE
    static constexpr bool use_debug_layers = true;
#else
    static constexpr bool use_debug_layers = false;
#endif

public:
    HYP_API static Engine *GetInstance();

    Engine();
    ~Engine();

    HYP_NODISCARD
    const RC<AppContext> &GetAppContext() const
        { return m_app_context; }

    HYP_NODISCARD HYP_FORCE_INLINE
    Instance *GetGPUInstance() const
        { return m_instance.Get(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    Device *GetGPUDevice() const
        { return m_instance ? m_instance->GetDevice() : nullptr; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    DeferredRenderer *GetDeferredRenderer() const
        { return m_deferred_renderer.Get(); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    GBuffer &GetGBuffer()
        { return m_gbuffer; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const GBuffer &GetGBuffer() const
        { return m_gbuffer; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    RenderState &GetRenderState()
        { return render_state; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const RenderState &GetRenderState() const
        { return render_state; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    ShaderGlobals *GetRenderData() const
        { return m_render_data.Get(); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    PlaceholderData *GetPlaceholderData() const
        { return m_placeholder_data.Get(); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const Handle<World> &GetWorld() const
        { return m_world; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    Configuration &GetConfig()
        { return m_configuration; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const Configuration &GetConfig() const
        { return m_configuration; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    ShaderCompiler &GetShaderCompiler()
        { return m_shader_compiler; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const ShaderCompiler &GetShaderCompiler() const
        { return m_shader_compiler; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    DebugDrawer &GetDebugDrawer()
        { return m_debug_drawer; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const DebugDrawer &GetDebugDrawer() const
        { return m_debug_drawer; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.At(type); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    FinalPass *GetFinalPass() const
        { return m_final_pass.Get(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    ScriptingService *GetScriptingService() const
        { return m_scripting_service.Get(); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const DescriptorTableRef &GetGlobalDescriptorTable() const
        { return m_global_descriptor_table; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    MaterialDescriptorSetManager &GetMaterialDescriptorSetManager()
        { return m_material_descriptor_set_manager; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const MaterialDescriptorSetManager &GetMaterialDescriptorSetManager() const
        { return m_material_descriptor_set_manager; }

    HYP_NODISCARD HYP_FORCE_INLINE
    EngineDelegates &GetDelegates()
        { return m_delegates; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const EngineDelegates &GetDelegates() const
        { return m_delegates; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsShuttingDown() const
        { return m_is_shutting_down.Get(MemoryOrder::SEQUENTIAL); }

    Handle<RenderGroup> CreateRenderGroup(
        const RenderableAttributeSet &renderable_attributes,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT
    );
    
    /*! \brief Create a RenderGroup using defined set of DescriptorSets. The result will not be cached. */
    Handle<RenderGroup> CreateRenderGroup(
        const ShaderRef &shader,
        const RenderableAttributeSet &renderable_attributes,
        const DescriptorTableRef &descriptor_table,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT
    );

    void AddRenderGroup(Handle<RenderGroup> &render_group);

    bool IsRenderLoopActive() const
        { return m_is_render_loop_active; }

    HYP_API void Initialize(const RC<AppContext> &app_context);
    HYP_API bool InitializeGame(Game *game);
    HYP_API void RenderNextFrame(Game *game);
    HYP_API void RequestStop();

    ShaderCompiler m_shader_compiler;

    RenderState render_state;

    AtomicVar<bool> m_stop_requested;

    template <class T, class ... Args>
    Handle<T> CreateObject(Args &&... args)
    {
        auto &container = Handle<T>::GetContainer();

        const uint index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<Args>(args)...
        );

        return Handle<T>(ID<T>::FromIndex(index));
    }

    template <class T>
    Handle<T> CreateObject()
    {
        auto &container = Handle<T>::GetContainer();

        const uint index = container.NextIndex();
        container.ConstructAtIndex(index);

        return Handle<T>(ID<T>::FromIndex(index));
    }

    template <class T>
    bool InitObject(Handle<T> &handle)
    {
        if (!handle) {
            return false;
        }

        if (!handle->GetID()) {
            return false;
        }

        handle->Init();

        return true;
    }

    void FinalizeStop();

private:
    void Compile();
    void ResetRenderState(RenderStateMask mask);
    void UpdateBuffersAndDescriptors(Frame *frame);

    void PreFrameUpdate(Frame *frame);
    void RenderDeferred(Frame *frame);

    void FindTextureFormatDefaults();

    void AddRenderGroupInternal(Handle<RenderGroup> &, bool cache);

    RC<AppContext>                                          m_app_context;
    
    UniquePtr<Instance>                                     m_instance;

    UniquePtr<PlaceholderData>                              m_placeholder_data;

    DescriptorTableRef                                      m_global_descriptor_table;

    MaterialDescriptorSetManager                            m_material_descriptor_set_manager;

    HashMap<TextureFormatDefault, InternalFormat>           m_texture_format_defaults;

    UniquePtr<DeferredRenderer>                             m_deferred_renderer;
    GBuffer                                                 m_gbuffer;
    FlatMap<RenderableAttributeSet, Handle<RenderGroup>>    m_render_group_mapping;
    std::mutex                                              m_render_group_mapping_mutex;

    UniquePtr<ShaderGlobals>                                m_render_data;

    Handle<World>                                           m_world;
    
    Configuration                                           m_configuration;

    DebugDrawer                                             m_debug_drawer;

    UniquePtr<FinalPass>                                    m_final_pass;

    UniquePtr<ScriptingService>                             m_scripting_service;

    CrashHandler                                            m_crash_handler;

    EngineDelegates                                         m_delegates;

    AtomicVar<bool>                                         m_is_shutting_down { false };
    bool                                                    m_is_render_loop_active { false };
};

} // namespace hyperion

#endif

