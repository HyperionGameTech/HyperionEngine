#ifndef HYPERION_V2_BACKEND_RENDERER_RENDER_OBJECT_HPP
#define HYPERION_V2_BACKEND_RENDERER_RENDER_OBJECT_HPP

#include <core/Core.hpp>
#include <core/Name.hpp>
#include <core/Containers.hpp>
#include <core/IDCreator.hpp>
#include <core/lib/ValueStorage.hpp>
#include <Threads.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>

#include <mutex>
#include <atomic>
#include <tuple>

namespace hyperion {
namespace renderer {

template <class T, PlatformType PLATFORM>
struct RenderObjectDefinition;

template <class T, PlatformType PLATFORM>
constexpr bool has_render_object_defined = implementation_exists<RenderObjectDefinition<T, PLATFORM>>;

template <class T, PlatformType PLATFORM>
static inline Name GetNameForRenderObject()
{
    static_assert(has_render_object_defined<T, PLATFORM>, "T must be a render object");

    return RenderObjectDefinition<T, PLATFORM>::GetNameForType();
}

template <class T, PlatformType PLATFORM>
class RenderObjectContainer
{
    using IDCreatorType = v2::IDCreator<PLATFORM>;

public:
    static constexpr SizeType max_size = RenderObjectDefinition<T, PLATFORM>::max_size;

    static_assert(has_render_object_defined<T, PLATFORM>, "T is not a render object for the render platform!");

    struct Instance
    {
        ValueStorage<T>     storage;
        AtomicVar<UInt16>   ref_count_strong;
        AtomicVar<UInt16>   ref_count_weak;
        bool                has_value;

        Instance()
            : ref_count_strong(0),
              ref_count_weak(0),
              has_value(false)
        {
        }

        Instance(const Instance &other)             = delete;
        Instance &operator=(const Instance &other)  = delete;
        Instance(Instance &&other) noexcept         = delete;
        Instance &operator=(Instance &&other)       = delete;

        ~Instance()
        {
            if (HasValue()) {
                storage.Destruct();
            }
        }

        UInt16 GetRefCountStrong() const
            { return ref_count_strong.Get(MemoryOrder::SEQUENTIAL); }

        UInt16 GetRefCountWeak() const
            { return ref_count_weak.Get(MemoryOrder::SEQUENTIAL); }

        template <class ...Args>
        T *Construct(Args &&... args)
        {
            AssertThrow(!HasValue());

            T *ptr = storage.Construct(std::forward<Args>(args)...);

            has_value = true;

            return ptr;
        }

        HYP_FORCE_INLINE void IncRefStrong()
            { ref_count_strong.Increment(1, MemoryOrder::RELAXED); }

        UInt DecRefStrong()
        {
            AssertThrow(GetRefCountStrong() != 0);

            UInt16 count;

            if ((count = ref_count_strong.Decrement(1, MemoryOrder::SEQUENTIAL)) == 1) {
                storage.Destruct();
                has_value = false;
            }

            return UInt(count) - 1;
        }

        HYP_FORCE_INLINE void IncRefWeak()
            { ref_count_weak.Increment(1, MemoryOrder::RELAXED); }

        UInt DecRefWeak()
        {
            AssertThrow(GetRefCountWeak() != 0);

            const UInt16 count = ref_count_weak.Decrement(1, MemoryOrder::SEQUENTIAL);

            return UInt(count) - 1;
        }

        HYP_FORCE_INLINE T &Get()
        {
            AssertThrowMsg(HasValue(), "Render object of type %s has no value!", GetNameForRenderObject<T, PLATFORM>().LookupString().Data());

            return storage.Get();
        }

    private:
        HYP_FORCE_INLINE bool HasValue() const
            { return has_value; }
    };

    RenderObjectContainer()
        : m_size(0)
    {
    }

    RenderObjectContainer(const RenderObjectContainer &other) = delete;
    RenderObjectContainer &operator=(const RenderObjectContainer &other) = delete;

    ~RenderObjectContainer() = default;

    HYP_FORCE_INLINE UInt NextIndex()
    {
        using RenderObjectDefinitionType = RenderObjectDefinition<T, PLATFORM>;

        const UInt index = IDCreatorType::template ForType<T>().NextID() - 1;

        AssertThrowMsg(
            index < RenderObjectDefinitionType::max_size,
            "Maximum number of RenderObject type allocated! Maximum: %llu\n",
            RenderObjectDefinitionType::max_size
        );

        return index;
    }

    HYP_FORCE_INLINE
    void IncRefStrong(UInt index)
        { m_data[index].IncRefStrong(); }

    HYP_FORCE_INLINE
    void DecRefStrong(UInt index)
    {
        if (m_data[index].DecRefStrong() == 0 && m_data[index].GetRefCountWeak() == 0) {
            IDCreatorType::template ForType<T>().FreeID(index + 1);
        }
    }

    HYP_FORCE_INLINE
    void IncRefWeak(UInt index)
        { m_data[index].IncRefWeak(); }

    HYP_FORCE_INLINE
    void DecRefWeak(UInt index)
    {
        if (m_data[index].DecRefWeak() == 0 && m_data[index].GetRefCountStrong() == 0) {
            IDCreatorType::template ForType<T>().FreeID(index + 1);
        }
    }

    HYP_FORCE_INLINE
    UInt16 GetRefCountStrong(UInt index)
        { return m_data[index].GetRefCountStrong(); }

    HYP_FORCE_INLINE
    UInt16 GetRefCountWeak(UInt index)
        { return m_data[index].GetRefCountWeak(); }

    HYP_FORCE_INLINE T &Get(UInt index)
        { return m_data[index].Get(); }
    
    template <class ...Args>
    HYP_FORCE_INLINE void ConstructAtIndex(UInt index, Args &&... args)
        { m_data[index].Construct(std::forward<Args>(args)...); }

private:
    HeapArray<Instance, max_size>   m_data;
    SizeType                        m_size;
};

template <class T, PlatformType PLATFORM>
class RenderObjectHandle_Strong;

template <class T, PlatformType PLATFORM>
class RenderObjectHandle_Weak;

template <PlatformType PLATFORM>
class RenderObjects
{
public:
    template <class T>
    static RenderObjectContainer<T, PLATFORM> &GetRenderObjectContainer()
    {
        static_assert(has_render_object_defined<T, PLATFORM>, "Not a valid render object");

        static RenderObjectContainer<T, PLATFORM> container;

        return container;
    }

    template <class T, class ...Args>
    static RenderObjectHandle_Strong<T, PLATFORM> Make(Args &&... args)
    {
        auto &container = GetRenderObjectContainer<T>();

        const UInt index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<Args>(args)...
        );

        return RenderObjectHandle_Strong<T, PLATFORM>::FromIndex(index + 1);
    }
};

template <class T, PlatformType PLATFORM>
class RenderObjectHandle_Strong
{
    static_assert(has_render_object_defined<T, PLATFORM>, "Not a valid render object");

    RenderObjectContainer<T, PLATFORM>  *_container = &RenderObjects<PLATFORM>::template GetRenderObjectContainer<T>();

public:

    static const RenderObjectHandle_Strong unset;

    static RenderObjectHandle_Strong FromIndex(UInt index)
    {
        RenderObjectHandle_Strong handle;
        handle.index = index;
        
        if (index != 0) {
            handle._container->IncRefStrong(index - 1);
        }

        return handle;
    }

    RenderObjectHandle_Strong()
        : index(0)
    {
    }

    RenderObjectHandle_Strong(const RenderObjectHandle_Strong &other)
        : index(other.index)
    {
        if (index != 0) {
            _container->IncRefStrong(index - 1);
        }
    }

    RenderObjectHandle_Strong &operator=(const RenderObjectHandle_Strong &other)
    {
        if (index != 0) {
            _container->DecRefStrong(index - 1);
        }

        index = other.index;

        if (index != 0) {
            _container->IncRefStrong(index - 1);
        }

        return *this;
    }

    RenderObjectHandle_Strong(RenderObjectHandle_Strong &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    RenderObjectHandle_Strong &operator=(RenderObjectHandle_Strong &&other) noexcept
    {
        if (index != 0) {
            _container->DecRefStrong(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~RenderObjectHandle_Strong()
    {
        if (index != 0) {
            _container->DecRefStrong(index - 1);
        }
    }

    T *operator->() const
        { return Get(); }

    T &operator*()
        { return *Get(); }

    const T &operator*() const
        { return *Get(); }

    bool operator!() const
        { return !IsValid(); }

    explicit operator bool() const
        { return IsValid(); }

    bool operator==(std::nullptr_t) const
        { return !IsValid(); }

    bool operator!=(std::nullptr_t) const
        { return IsValid(); }

    bool operator==(const RenderObjectHandle_Strong &other) const
        { return index == other.index; }

    bool operator!=(const RenderObjectHandle_Strong &other) const
        { return index == other.index; }

    bool operator<(const RenderObjectHandle_Strong &other) const
        { return index < other.index; }

    bool IsValid() const
        { return index != 0; }

    T *Get() const
    {
        if (index == 0) {
            return nullptr;
        }
        
        return &_container->Get(index - 1);
    }

    void Reset()
    {
        if (index != 0) {
            _container->DecRefStrong(index - 1);
        }

        index = 0;
    }

    operator T *() const
        { return Get(); }

    UInt index;
};

template <class T, PlatformType PLATFORM>
const RenderObjectHandle_Strong<T, PLATFORM> RenderObjectHandle_Strong<T, PLATFORM>::unset = RenderObjectHandle_Strong<T, PLATFORM>();


template <class T, PlatformType PLATFORM>
class RenderObjectHandle_Weak
{
    static_assert(has_render_object_defined<T, PLATFORM>, "Not a valid render object");

    RenderObjectContainer<T, PLATFORM>  *_container = &RenderObjects<PLATFORM>::template GetRenderObjectContainer<T>();

public:
    static const RenderObjectHandle_Weak unset;

    static RenderObjectHandle_Weak FromIndex(UInt index)
    {
        RenderObjectHandle_Weak handle;
        handle.index = index;
        
        if (index != 0) {
            handle._container->IncRefWeak(index - 1);
        }

        return handle;
    }

    RenderObjectHandle_Weak()
        : index(0)
    {
    }

    RenderObjectHandle_Weak(const RenderObjectHandle_Strong<T, PLATFORM> &other)
        : index(other.index)
    {
        if (index != 0) {
            _container->IncRefWeak(index - 1);
        }
    }

    RenderObjectHandle_Weak(const RenderObjectHandle_Weak &other)
        : index(other.index)
    {
        if (index != 0) {
            _container->IncRefWeak(index - 1);
        }
    }

    RenderObjectHandle_Weak &operator=(const RenderObjectHandle_Weak &other)
    {
        if (index != 0) {
            _container->DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            _container->IncRefWeak(index - 1);
        }

        return *this;
    }

    RenderObjectHandle_Weak(RenderObjectHandle_Weak &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    RenderObjectHandle_Weak &operator=(RenderObjectHandle_Weak &&other) noexcept
    {
        if (index != 0) {
            _container->DecRefWeak(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~RenderObjectHandle_Weak()
    {
        if (index != 0) {
            _container->DecRefWeak(index - 1);
        }
    }
        
    RenderObjectHandle_Strong<T, PLATFORM> Lock() const
    {
        if (index == 0) {
            return RenderObjectHandle_Strong<T, PLATFORM>::unset;
        }

        _container->IncRefStrong(index - 1);

        return RenderObjectHandle_Strong<T, PLATFORM>::FromIndex(index);
    }

    bool operator==(const RenderObjectHandle_Weak &other) const
        { return index == other.index; }

    bool operator!=(const RenderObjectHandle_Weak &other) const
        { return index == other.index; }

    bool operator<(const RenderObjectHandle_Weak &other) const
        { return index < other.index; }

    bool IsValid() const
        { return index != 0; }

    void Reset()
    {
        if (index != 0) {
            _container->DecRefWeak(index - 1);
        }

        index = 0;
    }

    UInt index;
};

template <class T, PlatformType PLATFORM>
const RenderObjectHandle_Weak<T, PLATFORM> RenderObjectHandle_Weak<T, PLATFORM>::unset = RenderObjectHandle_Weak<T, PLATFORM>();

} // namespace renderer

#define DEF_RENDER_OBJECT(T, _max_size) \
    namespace renderer { \
    class T; \
    template <> \
    struct RenderObjectDefinition< T, Platform::CURRENT > \
    { \
        static constexpr SizeType max_size = (_max_size); \
        \
        static Name GetNameForType() \
        { \
            static const Name name = HYP_NAME(T); \
            return name; \
        } \
    }; \
    } \
    using T##Ref = renderer::RenderObjectHandle_Strong< renderer::T, renderer::Platform::CURRENT >; \
    using T##WeakRef = renderer::RenderObjectHandle_Weak< renderer::T, renderer::Platform::CURRENT >

#define DEF_RENDER_PLATFORM_OBJECT(_platform, T, _max_size) \
    namespace renderer { \
    namespace platform { \
    template <PlatformType PLATFORM> \
    class T; \
    } \
    using T##_##_platform = platform::T<renderer::Platform::_platform>; \
    template <> \
    struct RenderObjectDefinition< T##_##_platform, renderer::Platform::_platform > \
    { \
        static constexpr SizeType max_size = (_max_size); \
        \
        static Name GetNameForType() \
        { \
            static const Name name = HYP_NAME(T); \
            return name; \
        } \
    }; \
    } \
    using T##Ref##_##_platform = renderer::RenderObjectHandle_Strong< renderer::T##_##_platform, renderer::Platform::_platform >; \
    using T##WeakRef##_##_platform = renderer::RenderObjectHandle_Weak< renderer::T##_##_platform, renderer::Platform::_platform >; \

template <class RefType, class ... Args>
static inline void DeferCreate(RefType ref, Args &&... args)
{
    struct RENDER_COMMAND(CreateRenderObject) : renderer::RenderCommand
    {
        using ArgsTuple = std::tuple<Args...>;
        RefType     ref;
        ArgsTuple   args;
        RENDER_COMMAND(CreateRenderObject)(RefType &&ref, Args &&... args)
            : ref(std::move(ref)),
              args(std::forward<Args>(args)...)
        {
        }

        virtual renderer::Result operator()()
        {
            return std::apply([this]<class ...OtherArgs>(OtherArgs &&... args)
            {
                return ref->Create(std::forward<OtherArgs>(args)...);
            }, std::move(args));
        }
    };

    if (!ref.IsValid()) {
        return;
    }

    if (v2::Threads::IsOnThread(v2::THREAD_RENDER)) {
        HYPERION_ASSERT_RESULT(ref->Create(std::forward<Args>(args)...));
        return;
    }

    PUSH_RENDER_COMMAND(CreateRenderObject, std::move(ref), std::forward<Args>(args)...);
}

#if HYP_VULKAN
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T##Ref = T##Ref##_VULKAN; \
    using T##WeakRef = T##WeakRef##_VULKAN
#elif HYP_WEBGPU
    using T##Ref = T##Ref##_WEBGPU; \
    using T##WeakRef = T##WeakRef##_WEBGPU
#endif

DEF_RENDER_OBJECT(DescriptorSet,       4096);

DEF_RENDER_PLATFORM_OBJECT(VULKAN, Device,              1);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, Image,               16384);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, ImageView,           65536);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, Sampler,             16384);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, GPUBuffer,           65536);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, CommandBuffer,       2048);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, ComputePipeline,     4096);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, GraphicsPipeline,    4096);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, RaytracingPipeline,  128);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, FramebufferObject,   8192);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, ShaderProgram,       2048);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, Attachment,          4096);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, AttachmentUsage,     8192);

DEF_RENDER_PLATFORM_OBJECT(WEBGPU, Device,              1);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, Image,               16384);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, ImageView,           65536);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, Sampler,             16384);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, GPUBuffer,           65536);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, CommandBuffer,       2048);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, ComputePipeline,     4096);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, GraphicsPipeline,    4096);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, RaytracingPipeline,  128);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, FramebufferObject,   8192);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, ShaderProgram,       2048);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, Attachment,          4096);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, AttachmentUsage,     8192);

DEF_CURRENT_PLATFORM_RENDER_OBJECT(Device);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(Image);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(ImageView);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(Sampler);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(GPUBuffer);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(CommandBuffer);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(ComputePipeline);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(GraphicsPipeline);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(RaytracingPipeline);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(FramebufferObject);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(ShaderProgram);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(Attachment);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(AttachmentUsage);

#undef DEF_RENDER_OBJECT
#undef DEF_RENDER_PLATFORM_OBJECT
#undef DEF_CURRENT_PLATFORM_RENDER_OBJECT

template <class T, renderer::PlatformType PLATFORM, class ... Args>
static inline renderer::RenderObjectHandle_Strong<T, PLATFORM> MakeRenderObject(Args &&... args)
    { return renderer::RenderObjects<PLATFORM>::template Make<T>(std::forward<Args>(args)...); }

template <class T, class ... Args>
static inline renderer::RenderObjectHandle_Strong<T, renderer::Platform::CURRENT> MakeRenderObject(Args &&... args)
    { return renderer::RenderObjects<renderer::Platform::CURRENT>::template Make<T>(std::forward<Args>(args)...); }

struct DeletionQueueBase
{
    TypeID              type_id;
    AtomicVar<UInt32>   num_items { 0 };
    std::mutex          mtx;

    virtual ~DeletionQueueBase() = default;
    virtual void Iterate() = 0;
};

template <renderer::PlatformType PLATFORM>
struct RenderObjectDeleter
{
    static constexpr UInt       initial_cycles_remaining = max_frames_in_flight + 1;
    static constexpr SizeType   max_queues = 63;

    static FixedArray<DeletionQueueBase *, max_queues + 1>  queues;
    static AtomicVar<UInt16>                                queue_index;

    template <class T>
    struct DeletionQueue : DeletionQueueBase
    {
        using Base = DeletionQueueBase;

        static constexpr UInt initial_cycles_remaining = max_frames_in_flight + 1;

        Array<Pair<renderer::RenderObjectHandle_Strong<T, PLATFORM>, UInt8>>    items;
        Queue<renderer::RenderObjectHandle_Strong<T, PLATFORM>>                 to_delete;

        DeletionQueue()
        {
            Base::type_id = TypeID::ForType<T>();
        }

        DeletionQueue(const DeletionQueue &other)               = delete;
        DeletionQueue &operator=(const DeletionQueue &other)    = delete;
        virtual ~DeletionQueue() override                       = default;

        virtual void Iterate() override
        {
            if (!Base::num_items.Get(MemoryOrder::ACQUIRE)) {
                return;
            }
            
            Base::mtx.lock();

            for (auto it = items.Begin(); it != items.End();) {
                if (--it->second == 0) {
                    to_delete.Push(std::move(it->first));

                    it = items.Erase(it);
                } else {
                    ++it;
                }
            }

            Base::num_items.Set(UInt32(items.Size()), MemoryOrder::RELEASE);

            Base::mtx.unlock();

            while (to_delete.Any()) {
                HYPERION_ASSERT_RESULT(to_delete.Front()->Destroy(v2::GetEngineDevice()));

                to_delete.Pop();
            }
        }

        void Push(renderer::RenderObjectHandle_Strong<T, PLATFORM> &&handle)
        {
            Base::num_items.Increment(1, MemoryOrder::RELAXED);

            std::lock_guard guard(Base::mtx);

            items.PushBack({ std::move(handle), initial_cycles_remaining });
        }
    };

    template <class T>
    struct DeletionQueueInstance
    {
        DeletionQueue<T>    queue;
        UInt16              index;

        DeletionQueueInstance()
        {
            index = queue_index.Increment(1, MemoryOrder::RELAXED);

            AssertThrowMsg(index < max_queues, "Maximum number of deletion queues added");

            queues[index] = &queue;
        }

        DeletionQueueInstance(const DeletionQueueInstance &other) = delete;
        DeletionQueueInstance &operator=(const DeletionQueueInstance &other) = delete;
        DeletionQueueInstance(DeletionQueueInstance &&other) noexcept = delete;
        DeletionQueueInstance &operator=(DeletionQueueInstance &&other) noexcept = delete;

        ~DeletionQueueInstance()
        {
            queues[index] = nullptr;
        }
    };

    template <class T>
    static DeletionQueue<T> &GetQueue()
    {
        static DeletionQueueInstance<T> instance;

        return instance.queue;
    }

    static void Iterate()
    {
        DeletionQueueBase **queue = queues.Data();

        while (*queue) {
            (*queue)->Iterate();
            ++queue;
        }
    }
};

template <renderer::PlatformType PLATFORM>
FixedArray<DeletionQueueBase *, RenderObjectDeleter<PLATFORM>::max_queues + 1> RenderObjectDeleter<PLATFORM>::queues = { };

template <renderer::PlatformType PLATFORM>
AtomicVar<UInt16> RenderObjectDeleter<PLATFORM>::queue_index = { 0 };

template <class T, renderer::PlatformType PLATFORM>
static inline void SafeRelease(renderer::RenderObjectHandle_Strong<T, PLATFORM> &&handle)
{
    RenderObjectDeleter<PLATFORM>::template GetQueue<T>().Push(std::move(handle));
}

template <class T, SizeType Sz, renderer::PlatformType PLATFORM>
static inline void SafeRelease(Array<renderer::RenderObjectHandle_Strong<T, PLATFORM>, Sz> &&handles)
{
    for (auto &it : handles) {
        RenderObjectDeleter<PLATFORM>::template GetQueue<T>().Push(std::move(it));
    }
}

template <class T, SizeType Sz, renderer::PlatformType PLATFORM>
static inline void SafeRelease(FixedArray<renderer::RenderObjectHandle_Strong<T, PLATFORM>, Sz> &&handles)
{
    for (auto &it : handles) {
        RenderObjectDeleter<PLATFORM>::template GetQueue<T>().Push(std::move(it));
    }
}

} // namespace hyperion

#endif