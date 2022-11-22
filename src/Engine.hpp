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
#include <rendering/FinalPass.hpp>
#include <rendering/debug/ImmediateMode.hpp>
#include <scene/World.hpp>
#include <scene/System.hpp>

#include "GameThread.hpp"
#include "Threads.hpp"
#include "TaskSystem.hpp"

#include <core/Scheduler.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <TaskSystem.hpp>

#include <system/CrashHandler.hpp>

#include <util/EnumOptions.hpp>
#include <util/shader_compiler/ShaderCompiler.hpp>

#include <Component.hpp>

#include <Types.hpp>

#include <memory>
#include <mutex>
#include <stack>

#define HYP_FLUSH_RENDER_QUEUE(engine) \
    do { \
        (engine)->render_scheduler.FlushOrWait([](auto &fn) { \
            HYPERION_ASSERT_RESULT(fn(nullptr, 0)); \
        }); \
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




#define RENDER_COMMAND(name) RenderCommand_##name

template <class Derived>
struct RenderCommand;

struct RenderCommandDataBase
{
    static constexpr SizeType size = 512;
};

template <class T>
struct RenderCommandData : RenderCommandDataBase
{
};

struct RenderCommandBase
{
    UByte buffer[RenderCommandDataBase::size];

    Result(*fnptr)(RenderCommandBase *self, CommandBuffer *command_buffer, UInt frame_index);
    void(*delete_ptr)(RenderCommandBase *self);
    
    RenderCommandBase()
        : fnptr(nullptr),
          delete_ptr(nullptr)
    {
        Memory::Clear(buffer, sizeof(buffer));
    }

    RenderCommandBase(const RenderCommandBase &other) = delete;
    RenderCommandBase &operator=(const RenderCommandBase &other) = delete;

    RenderCommandBase(RenderCommandBase &&other) noexcept
        : fnptr(other.fnptr),
          delete_ptr(other.delete_ptr)
    {
        Memory::Move(buffer, other.buffer, sizeof(buffer));

        other.fnptr = nullptr;
        other.delete_ptr = nullptr;
    }

    RenderCommandBase &operator=(RenderCommandBase &&other) noexcept
    {
        if (IsValid()) {
            delete_ptr(this);
        }

        Memory::Move(buffer, other.buffer, sizeof(buffer));

        fnptr = other.fnptr;
        other.fnptr = nullptr;

        delete_ptr = other.delete_ptr;
        other.delete_ptr = nullptr;

        return *this;
    }

    ~RenderCommandBase()
    {
        if (IsValid()) {
            delete_ptr(this);
        }
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return fnptr != nullptr;
    }

    HYP_FORCE_INLINE Result operator()(CommandBuffer *command_buffer, UInt frame_index)
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(IsValid());
#endif

        return fnptr(this, command_buffer, frame_index);
    }
};

template <class Derived, class ReturnType, class ...Args>
static ReturnType InvokeRenderCommand(RenderCommandBase *self, Args ...args)
{
    alignas(alignof(RenderCommandData<Derived>)) UByte render_command_data[sizeof(RenderCommandData<Derived>)];
    Memory::Copy(render_command_data, self->buffer, sizeof(RenderCommandData<Derived>));

    return static_cast<Derived *>(self)->Call(reinterpret_cast<RenderCommandData<Derived> *>(&render_command_data[0]), args...);
}

template <class Derived>
static void DestroyRenderCommand(RenderCommandBase *self)
{
    using DataType = RenderCommandData<Derived>;

    alignas(alignof(DataType)) UByte render_command_data[sizeof(DataType)];
    Memory::Copy(render_command_data, self->buffer, sizeof(DataType));

    reinterpret_cast<DataType *>(render_command_data)->~DataType();

    Memory::Clear(self->buffer, sizeof(DataType));
}

template <class Derived>
struct RenderCommand : RenderCommandBase
{
    RenderCommand()
    {
        static_assert(sizeof(RenderCommandData<Derived>) <= RenderCommandDataBase::size, "sizeof(RenderCommandData<Derived>) must be <= RenderCommandDataBase::size!");

        RenderCommandBase::fnptr = &InvokeRenderCommand<Derived, Result, CommandBuffer *, UInt>;
        RenderCommandBase::delete_ptr = &DestroyRenderCommand<Derived>;

        Memory::Clear(buffer, sizeof(buffer));
    }

    /*RenderCommand(RenderCommandData<Derived> &&data)
        : RenderCommand()
    {
        alignas(alignof(RenderCommandData<Derived>)) UByte render_command_data[sizeof(RenderCommandData<Derived>)];
        new (render_command_data) RenderCommandData<Derived>(std::move(data));

        Memory::Move(buffer, render_command_data, sizeof(sizeof(RenderCommandData<Derived>));
    }*/

    RenderCommand(const RenderCommand &other) = delete;
    RenderCommand &operator=(const RenderCommand &other) = delete;

    RenderCommand(RenderCommand &&other) noexcept
        : RenderCommandBase(std::move(other))
    {
    }

    RenderCommand &operator=(RenderCommand &&other) noexcept
    {
        RenderCommandBase::operator=(std::move(other));

        return *this;
    }

    operator RenderCommandBase() const
    {
        static_assert(sizeof(RenderCommand<Derived>) == sizeof(RenderCommandBase));
        return RenderCommandBase(static_cast<const RenderCommandBase &>(*this));
    }
};

struct RenderCommand_UpdateEntityData;

template <>
struct RenderCommandData<RenderCommand_UpdateEntityData>
{
    ObjectShaderData shader_data;
};

struct RENDER_COMMAND(UpdateEntityData) : RenderCommand<RENDER_COMMAND(UpdateEntityData)>
{
    Result Call(RenderCommandData<RenderCommand_UpdateEntityData> *data, CommandBuffer * command_buffer, UInt frame_index)
    {
        data->shader_data.bucket = Bucket::BUCKET_INTERNAL;

        volatile int y = 0;

        for (int x = 0; x < 100; x++) {
            ++y;
        }

        HYPERION_RETURN_OK;
    }
};


struct RenderCommandBase2
{
    virtual ~RenderCommandBase2() = default;

    virtual Result operator()(CommandBuffer * command_buffer, UInt frame_index) = 0;
};

struct RenderCommand2_UpdateEntityData : RenderCommandBase2
{
    RenderCommandData<RenderCommand_UpdateEntityData> data;

    virtual Result operator()(CommandBuffer *command_buffer, UInt frame_index)
    {
        data.shader_data.bucket = Bucket::BUCKET_INTERNAL;

        volatile int y = 0;

        for (int x = 0; x < 100; x++) {
            ++y;
        }

        HYPERION_RETURN_OK;
    }
};



using RenderFunctor = Task<Result, CommandBuffer * /* command_buffer */, UInt /* frame_index */>;

/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine
{
#ifdef HYP_DEBUG_MODE
    static constexpr bool use_debug_layers = true;
#else
    static constexpr bool use_debug_layers = false;
#endif

public:
    Engine(RefCountedPtr<Application> application, const char *app_name);
    ~Engine();
    
    Instance *GetInstance() const { return m_instance.get(); }
    Device *GetDevice() const { return m_instance ? m_instance->GetDevice() : nullptr; }

    DeferredRenderer &GetDeferredRenderer() { return m_deferred_renderer; }
    const DeferredRenderer &GetDeferredRenderer() const { return m_deferred_renderer; }

    DeferredSystem &GetDeferredSystem() { return m_render_list_container; }
    const DeferredSystem &GetDeferredSystem() const { return m_render_list_container; }

    auto &GetRenderScheduler() { return render_scheduler; }
    const auto &GetRenderScheduler() const { return render_scheduler; }

    RenderState &GetRenderState() { return render_state; }
    const RenderState &GetRenderState() const { return render_state; }

    ShaderGlobals *GetRenderData() const { return shader_globals; }
    
    PlaceholderData &GetPlaceholderData() { return m_placeholder_data; }
    const PlaceholderData &GetPlaceholderData() const { return m_placeholder_data; }
    
    ComponentSystem &GetObjectSystem() { return registry; }
    const ComponentSystem &GetObjectSystem() const { return registry; }

    AssetManager &GetAssetManager() { return m_asset_manager; }
    const AssetManager &GetAssetManager() const { return m_asset_manager; }

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

    Handle<RendererInstance> CreateRendererInstance(
        const Handle<Shader> &shader,
        const RenderableAttributeSet &renderable_attributes,
        bool cache = false
    );
    
    /*! \brief Create a RendererInstance using defined set of DescriptorSets. The result will not be cached. */
    Handle<RendererInstance> CreateRendererInstance(
        const Handle<Shader> &shader,
        const RenderableAttributeSet &renderable_attributes,
        const Array<const DescriptorSet *> &used_descriptor_sets
    );

    /*! \brief Find or create a RendererInstance from cache, or create a descriptor set. If created, the result will always be cached. */
    Handle<RendererInstance> FindOrCreateRendererInstance(const Handle<Shader> &shader, const RenderableAttributeSet &renderable_attributes);
    Handle<RendererInstance> AddRendererInstance(std::unique_ptr<RendererInstance> &&renderer_instance);

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

    void Initialize();
    void Compile();
    void RequestStop();

    // temporarily public: RT stuff is not yet in it's own unit,
    // needs to use this method
    void PreFrameUpdate(Frame *frame);
    // temporarily public: RT stuff is not yet in it's own unit,
    // needs to use this method
    void RenderDeferred(Frame *frame);
    // temporarily public: RT stuff is not yet in it's own unit,
    // needs to use this method
    void RenderFinalPass(Frame *frame);

    void RenderNextFrame(Game *game);

    ShaderGlobals *shader_globals;

    EngineCallbacks callbacks;

    ShaderManager shader_manager;
    ShaderCompiler m_shader_compiler;
                             
    RenderState render_state;
    
    std::atomic_bool m_running { false };

    Scheduler<RenderFunctor> render_scheduler;

    GameThread game_thread;
    TaskSystem task_system;

    template <class T, class First, class Second, class ...Rest>
    Handle<T> CreateHandle(First &&first, Second &&second, Rest &&... args)
    {
        Handle<T> handle(new T(std::forward<First>(first), std::forward<Second>(second), std::forward<Rest>(args)...));
        registry.template Attach<T>(handle);

        return handle;
    }

    template <class T, class First>
    typename std::enable_if_t<
        (!std::is_pointer_v<std::remove_reference_t<First>> || !std::is_convertible_v<std::remove_reference_t<First>, std::add_pointer_t<T>>)
            && !std::is_base_of_v<AtomicRefCountedPtr<T>, std::remove_reference_t<First>>
            && !std::is_base_of_v<UniquePtr<T>, std::remove_reference_t<First>>,
        Handle<T>
    >
    CreateHandle(First &&first)
    {
        Handle<T> handle(new T(std::forward<First>(first)));
        registry.template Attach<T>(handle);

        return handle;
    }

    template <class T, class Ptr>
    typename std::enable_if_t<
        std::is_pointer_v<std::remove_reference_t<Ptr>> && std::is_convertible_v<std::remove_reference_t<Ptr>, std::add_pointer_t<T>>,
        Handle<T>
    >
    CreateHandle(Ptr &&ptr)
    {
        Handle<T> handle(static_cast<T *>(ptr));
        registry.template Attach<T>(handle);

        return handle;
    }

    template <class T, class Ptr>
    typename std::enable_if_t<
        std::is_base_of_v<UniquePtr<T>, std::remove_reference_t<Ptr>>,
        Handle<T>
    >
    CreateHandle(Ptr &&ptr)
    {
        Handle<T> handle(ptr.template MakeRefCounted<std::atomic<UInt>>());
        registry.template Attach<T>(handle);

        return handle;
    }

    template <class T, class RC>
    typename std::enable_if_t<
        std::is_base_of_v<AtomicRefCountedPtr<T>, std::remove_reference_t<RC>>,
        Handle<T>
    >
    CreateHandle(RC &&rc)
    {
        Handle<T> handle(rc.template Cast<T>());
        registry.template Attach<T>(handle);

        return handle;
    }

    template <class T>
    Handle<T> CreateHandle()
    {
        Handle<T> handle(new T());
        registry.template Attach<T>(handle);

        return handle;
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

        handle->Init(this);

        return true;
    }

    template <class T>
    bool InitObject(WeakHandle<T> &handle)
    {
        auto locked = handle.Lock();

        return InitObject(locked);
    }

    template <class T>
    void Attach(Handle<T> &handle, bool init = true)
    {
        if (!handle) {
            return;
        }

        registry.template Attach<T>(handle);

        if (init) {
            handle->Init(this);
        }
    }

    ComponentSystem registry;

private:
    void RegisterDefaultAssetLoaders();

    void FinalizeStop();

    void ResetRenderState(RenderStateMask mask);
    void UpdateBuffersAndDescriptors(UInt frame_index);

    void PrepareFinalPass();

    void FindTextureFormatDefaults();

    void AddRendererInstanceInternal(Handle<RendererInstance> &);
    
    std::unique_ptr<Instance> m_instance;
    Handle<RendererInstance> m_root_pipeline;

    EnumOptions<TextureFormatDefault, InternalFormat, 16> m_texture_format_defaults;

    DeferredRenderer m_deferred_renderer;
    DeferredSystem m_render_list_container;

    /* TMP */
    std::vector<std::unique_ptr<renderer::Attachment>> m_render_pass_attachments;

    FlatMap<RenderableAttributeSet, WeakHandle<RendererInstance>> m_renderer_instance_mapping;
    std::mutex m_renderer_instance_mapping_mutex;

    ComponentRegistry m_components;

    PlaceholderData m_placeholder_data;

    SafeDeleter m_safe_deleter;

    Handle<World> m_world;

    Handle<Mesh> m_full_screen_quad;

    AssetManager m_asset_manager;
    Configuration m_configuration;

    ImmediateMode m_immediate_mode;

    CrashHandler m_crash_handler;

    bool m_is_stopping { false };
    bool m_is_render_loop_active { false };
};

} // namespace hyperion::v2

#endif

