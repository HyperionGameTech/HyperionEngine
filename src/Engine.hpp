#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include <Config.hpp>

#include <asset/Assets.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DeferredSystem.hpp>
#include <rendering/Shadows.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/DefaultFormats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/RenderCommands.hpp>
#include <rendering/debug/ImmediateMode.hpp>
#include <rendering/Material.hpp>
#include <rendering/FinalPass.hpp>
#include <scene/World.hpp>
#include <scene/System.hpp>

#include <GameThread.hpp>
#include <Threads.hpp>
#include <TaskSystem.hpp>

#include <core/Scheduler.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/ObjectPool.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <system/CrashHandler.hpp>

#include <util/EnumOptions.hpp>
#include <util/shader_compiler/ShaderCompiler.hpp>

#include <Types.hpp>

#include <memory>
#include <mutex>
#include <stack>

#define HYP_SYNC_RENDER() \
    do { \
        Threads::AssertOnThread(~THREAD_TASK, "Waiting on render thread from task threads is disabled as it may cause a deadlock."); \
        HYPERION_ASSERT_RESULT(RenderCommands::FlushOrWait()); \
    } while (0)

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;
using renderer::StorageBuffer;

class Engine;
class Game;
class Framebuffer;

extern Engine *g_engine;
extern AssetManager *g_asset_manager;
extern ShaderManagerSystem *g_shader_manager;
extern MaterialCache *g_material_system;

struct DebugMarker
{
    CommandBuffer *command_buffer = nullptr;
    const char * const name = "<Unnamed debug marker>";
    bool is_ended = false;

    DebugMarker(CommandBuffer *command_buffer, const char *marker_name)
        : command_buffer(command_buffer),
          name(marker_name)
    {
        if (command_buffer != nullptr) {
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

class IndirectDrawState;

class Engine
{
#ifdef HYP_DEBUG_MODE
    static constexpr bool use_debug_layers = true;
#else
    static constexpr bool use_debug_layers = false;
#endif

public:
    HYP_FORCE_INLINE static Engine *Get()
        { return g_engine; }

    Engine();
    ~Engine();

    bool InitializeGame(Game *game);
    
    Instance *GetGPUInstance() const { return m_instance.Get(); }
    Device *GetGPUDevice() const { return m_instance ? m_instance->GetDevice() : nullptr; }

    DeferredRenderer &GetDeferredRenderer() { return m_deferred_renderer; }
    const DeferredRenderer &GetDeferredRenderer() const { return m_deferred_renderer; }

    DeferredSystem &GetDeferredSystem() { return m_render_list_container; }
    const DeferredSystem &GetDeferredSystem() const { return m_render_list_container; }

    RenderState &GetRenderState() { return render_state; }
    const RenderState &GetRenderState() const { return render_state; }

    ShaderGlobals *GetRenderData() const { return shader_globals; }
    
    PlaceholderData &GetPlaceholderData() { return m_placeholder_data; }
    const PlaceholderData &GetPlaceholderData() const { return m_placeholder_data; }
    
    ObjectPool &GetObjectPool() { return registry; }
    const ObjectPool &GetObjectPool() const { return registry; }

    Handle<World> &GetWorld() { return m_world; }
    const Handle<World> &GetWorld() const { return m_world; }

    Configuration &GetConfig() { return m_configuration; }
    const Configuration &GetConfig() const { return m_configuration; }

    ShaderCompiler &GetShaderCompiler() { return m_shader_compiler; }
    const ShaderCompiler &GetShaderCompiler() const { return m_shader_compiler; }

    ComponentRegistry &GetComponents() { return m_components; }
    const ComponentRegistry &GetComponents() const { return m_components; }

    ImmediateMode &GetImmediateMode() { return m_immediate_mode; }
    const ImmediateMode &GetImmediateMode() const { return m_immediate_mode; }

    InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    const FinalPass &GetFinalPass() const
        { return m_final_pass; }

    Handle<RenderGroup> CreateRenderGroup(
        const RenderableAttributeSet &renderable_attributes
    );
    
    /*! \brief Create a RenderGroup using defined set of DescriptorSets. The result will not be cached. */
    Handle<RenderGroup> CreateRenderGroup(
        const Handle<Shader> &shader,
        const RenderableAttributeSet &renderable_attributes,
        const Array<DescriptorSetRef> &used_descriptor_sets
    );


    void AddRenderGroup(Handle<RenderGroup> &render_group);

    const auto &GetRenderGroupMapping() const
        { return m_render_group_mapping; }

    template <class T>
    void SafeReleaseHandle(Handle<T> &&resource)
    {
        m_safe_deleter.SafeReleaseHandle(std::move(resource));
    }

    template <class T>
    void SafeRelease(UniquePtr<T> &&object)
    {
        m_safe_deleter.SafeRelease(std::move(object));
    }

    bool IsRenderLoopActive() const
        { return m_is_render_loop_active; }

    void Initialize(RC<Application> application);
    void Compile();
    void RequestStop();

    void PreFrameUpdate(Frame *frame);
    void RenderDeferred(Frame *frame);

    void RenderNextFrame(Game *game);

    ShaderGlobals *shader_globals;

    EngineCallbacks callbacks;

    ShaderCompiler m_shader_compiler;

    RenderState render_state;

    AtomicVar<Bool> m_stop_requested;

    UniquePtr<GameThread> game_thread;

    template <class T, class First, class Second, class ...Rest>
    Handle<T> CreateObject(First &&first, Second &&second, Rest &&... args)
    {
        auto &container = GetObjectPool().GetContainer<T>();

        const UInt index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<First>(first),
            std::forward<Second>(second),
            std::forward<Rest>(args)...
        );

        return Handle<T>(ID<T>(index + 1));
    }

    template <class T, class First>
    typename std::enable_if_t<
        (!std::is_pointer_v<std::remove_reference_t<First>> || !std::is_convertible_v<std::remove_reference_t<First>, std::add_pointer_t<T>>)
            && !std::is_base_of_v<AtomicRefCountedPtr<T>, std::remove_reference_t<First>>
            && !std::is_base_of_v<UniquePtr<T>, std::remove_reference_t<First>>,
        Handle<T>
    >
    CreateObject(First &&first)
    {
        auto &container = GetObjectPool().GetContainer<T>();

        const UInt index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<First>(first)
        );

        return Handle<T>(ID<T>(index + 1));
    }

    template <class T>
    Handle<T> CreateObject()
    {
        auto &container = GetObjectPool().GetContainer<T>();

        const UInt index = container.NextIndex();
        container.ConstructAtIndex(index);

        return Handle<T>(ID<T>(index + 1));
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

    ObjectPool registry;

private:
    void RegisterComponents();

    void FinalizeStop();

    void ResetRenderState(RenderStateMask mask);
    void UpdateBuffersAndDescriptors(UInt frame_index);

    void FindTextureFormatDefaults();

    void AddRenderGroupInternal(Handle<RenderGroup> &, bool cache);
    
    UniquePtr<Instance> m_instance;

    EnumOptions<TextureFormatDefault, InternalFormat, 16> m_texture_format_defaults;

    DeferredRenderer m_deferred_renderer;
    DeferredSystem m_render_list_container;
    FlatMap<RenderableAttributeSet, Handle<RenderGroup>> m_render_group_mapping;
    std::mutex m_render_group_mapping_mutex;

    ComponentRegistry m_components;

    PlaceholderData m_placeholder_data;

    SafeDeleter m_safe_deleter;

    Handle<World> m_world;
    
    Configuration m_configuration;

    ImmediateMode m_immediate_mode;

    FinalPass m_final_pass;

    CrashHandler m_crash_handler;

    bool m_is_stopping { false };
    bool m_is_render_loop_active { false };
};

} // namespace hyperion::v2

#endif

