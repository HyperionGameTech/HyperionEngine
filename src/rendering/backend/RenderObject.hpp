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

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>

#include <mutex>
#include <atomic>

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
        AtomicVar<UInt16>   ref_count;
        bool                has_value;

        Instance()
            : ref_count(0),
              has_value(false)
        {
        }

        Instance(const Instance &other) = delete;
        Instance &operator=(const Instance &other) = delete;
        Instance(Instance &&other) noexcept = delete;
        Instance &operator=(Instance &&other) = delete;

        ~Instance()
        {
            if (HasValue()) {
                storage.Destruct();
            }
        }

        UInt16 GetRefCount() const
        {
            return ref_count.Get(MemoryOrder::SEQUENTIAL);
        }

        template <class ...Args>
        T *Construct(Args &&... args)
        {
            AssertThrow(!HasValue());

            T *ptr = storage.Construct(std::forward<Args>(args)...);

            has_value = true;

            return ptr;
        }

        HYP_FORCE_INLINE void IncRef()
            { ref_count.Increment(1, MemoryOrder::RELAXED); }

        UInt DecRef()
        {
            AssertThrow(GetRefCount() != 0);

            UInt16 count;

            if ((count = ref_count.Decrement(1, MemoryOrder::SEQUENTIAL)) == 1) {
                storage.Destruct();
                has_value = false;
            }

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

    HYP_FORCE_INLINE void IncRef(UInt index)
    {
        m_data[index].IncRef();
    }

    HYP_FORCE_INLINE void DecRef(UInt index)
    {
        if (m_data[index].DecRef() == 0) {
            IDCreatorType::template ForType<T>().FreeID(index + 1);
        }
    }

    HYP_FORCE_INLINE UInt16 GetRefCount(UInt index)
    {
        return m_data[index].GetRefCount();
    }

    HYP_FORCE_INLINE T &Get(UInt index)
    {
        return m_data[index].Get();
    }
    
    template <class ...Args>
    HYP_FORCE_INLINE void ConstructAtIndex(UInt index, Args &&... args)
    {
        m_data[index].Construct(std::forward<Args>(args)...);
    }

private:
    HeapArray<Instance, max_size>   m_data;
    SizeType                        m_size;
};

template <class T, PlatformType PLATFORM>
class RenderObjectHandle;

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
    static RenderObjectHandle<T, PLATFORM> Make(Args &&... args)
    {
        auto &container = GetRenderObjectContainer<T>();

        const UInt index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<Args>(args)...
        );

        return RenderObjectHandle<T, PLATFORM>::FromIndex(index + 1);
    }
};

template <class T, PlatformType PLATFORM>
class RenderObjectHandle
{
    static_assert(has_render_object_defined<T, PLATFORM>, "Not a valid render object");

    UInt                                index;
    RenderObjectContainer<T, PLATFORM>  *_container = &RenderObjects<PLATFORM>::template GetRenderObjectContainer<T>();

public:

    static const RenderObjectHandle unset;

    static RenderObjectHandle FromIndex(UInt index)
    {
        RenderObjectHandle handle;
        handle.index = index;
        
        if (index != 0) {
            handle._container->IncRef(index - 1);
        }

        AssertThrow(handle.GetRefCount() != 0);

        return handle;
    }

    RenderObjectHandle()
        : index(0)
    {
    }

    RenderObjectHandle(const RenderObjectHandle &other)
        : index(other.index)
    {
        if (index != 0) {
            _container->IncRef(index - 1);
        }
    }

    RenderObjectHandle &operator=(const RenderObjectHandle &other)
    {
        if (index != 0) {
            _container->DecRef(index - 1);
        }

        index = other.index;

        if (index != 0) {
            _container->IncRef(index - 1);
        }

        return *this;
    }

    RenderObjectHandle(RenderObjectHandle &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    RenderObjectHandle &operator=(RenderObjectHandle &&other) noexcept
    {
        if (index != 0) {
            _container->DecRef(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~RenderObjectHandle()
    {
        if (index != 0) {
            _container->DecRef(index - 1);
        }
    }

    HYP_FORCE_INLINE static Name GetTypeName()
        { return GetNameForRenderObject<T, PLATFORM>(); }

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

    bool operator==(const RenderObjectHandle &other) const
        { return index == other.index; }

    bool operator!=(const RenderObjectHandle &other) const
        { return index == other.index; }

    bool operator<(const RenderObjectHandle &other) const
        { return index < other.index; }

    bool IsValid() const
        { return index != 0; }

    UInt16 GetRefCount() const
    {
        return index == 0 ? 0 : _container->GetRefCount(index - 1);
    }

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
            _container->DecRef(index - 1);
        }

        index = 0;
    }

    operator T *() const
        { return Get(); }
};

template <class T, PlatformType PLATFORM>
const RenderObjectHandle<T, PLATFORM> RenderObjectHandle<T, PLATFORM>::unset = RenderObjectHandle<T, PLATFORM>();

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
    using T##Ref = renderer::RenderObjectHandle< renderer::T, renderer::Platform::CURRENT >

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
    using T##Ref##_##_platform = renderer::RenderObjectHandle< renderer::T##_##_platform, renderer::Platform::_platform >

#if HYP_VULKAN
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T##Ref = T##Ref##_VULKAN
#elif HYP_WEBGPU
    using T##Ref = T##Ref##_WEBGPU
#endif

DEF_RENDER_OBJECT(ShaderProgram,       2048);
DEF_RENDER_OBJECT(GraphicsPipeline,    2048);
DEF_RENDER_OBJECT(ImageView,           65536);
DEF_RENDER_OBJECT(Sampler,             16384);
DEF_RENDER_OBJECT(RaytracingPipeline,  128);
DEF_RENDER_OBJECT(DescriptorSet,       4096);
DEF_RENDER_OBJECT(ComputePipeline,     4096);
DEF_RENDER_OBJECT(Attachment,          4096);
DEF_RENDER_OBJECT(AttachmentUsage,     8192);
DEF_RENDER_OBJECT(FramebufferObject,   8192);

DEF_RENDER_PLATFORM_OBJECT(VULKAN, Device,          1);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, Image,           16384);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, GPUBuffer,       65536);
DEF_RENDER_PLATFORM_OBJECT(VULKAN, CommandBuffer,   2048);

DEF_RENDER_PLATFORM_OBJECT(WEBGPU, Device,          1);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, Image,           16384);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, GPUBuffer,       65536);
DEF_RENDER_PLATFORM_OBJECT(WEBGPU, CommandBuffer,   2048);

DEF_CURRENT_PLATFORM_RENDER_OBJECT(Device);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(Image);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(GPUBuffer);
DEF_CURRENT_PLATFORM_RENDER_OBJECT(CommandBuffer);

#undef DEF_RENDER_OBJECT
#undef DEF_RENDER_PLATFORM_OBJECT

template <class T, renderer::PlatformType PLATFORM, class ... Args>
static inline renderer::RenderObjectHandle<T, PLATFORM> MakeRenderObject(Args &&... args)
    { return renderer::RenderObjects<PLATFORM>::template Make<T>(std::forward<Args>(args)...); }

template <class T, class ... Args>
static inline renderer::RenderObjectHandle<T, renderer::Platform::CURRENT> MakeRenderObject(Args &&... args)
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

        Array<Pair<renderer::RenderObjectHandle<T, PLATFORM>, UInt8>> items;
        Queue<renderer::RenderObjectHandle<T, PLATFORM>>              to_delete;

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

        void Push(renderer::RenderObjectHandle<T, PLATFORM> &&handle)
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

// template <class T, renderer::PlatformType PLATFORM>
// struct RenderObjectDeletionEntry : private KeyValuePair<renderer::RenderObjectHandle<T, PLATFORM>, UInt8>
// {
//     using Base = KeyValuePair<renderer::RenderObjectHandle<T, PLATFORM>, UInt8>;

//     using Base::first;
//     using Base::second;

//     static_assert(renderer::has_render_object_defined<T, PLATFORM>, "T must be a render object");

//     RenderObjectDeletionEntry(renderer::RenderObjectHandle<T, PLATFORM> &&renderable)
//         : Base(std::move(renderable), initial_cycles_remaining)
//     {
//     }

//     RenderObjectDeletionEntry(const renderer::RenderObjectHandle<T, PLATFORM> &renderable) = delete;

//     RenderObjectDeletionEntry(const RenderObjectDeletionEntry &other) = delete;
//     RenderObjectDeletionEntry &operator=(const RenderObjectDeletionEntry &other) = delete;

//     RenderObjectDeletionEntry(RenderObjectDeletionEntry &&other) noexcept
//         : Base(std::move(other))
//     {
//     }

//     RenderObjectDeletionEntry &operator=(RenderObjectDeletionEntry &&other) noexcept
//     {
//         Base::operator=(std::move(other));

//         return *this;
//     }

//     ~RenderObjectDeletionEntry() = default;

//     bool operator==(const RenderObjectDeletionEntry &other) const
//         { return Base::operator==(other); }

//     bool operator<(const RenderObjectDeletionEntry &other) const
//         { return Base::operator<(other); }

//     void PerformDeletion(bool force = false)
//     {
//         // cycle should be at zero
//         AssertThrow(force || Base::second == 0u);

//         Base::first.Reset();
//     }
// };

template <class T, renderer::PlatformType PLATFORM>
static inline void SafeRelease(renderer::RenderObjectHandle<T, PLATFORM> &&handle)
{
    RenderObjectDeleter<PLATFORM>::template GetQueue<T>().Push(std::move(handle));
}

template <class T, SizeType Sz, renderer::PlatformType PLATFORM>
static inline void SafeRelease(Array<renderer::RenderObjectHandle<T, PLATFORM>, Sz> &&handles)
{
    for (auto &it : handles) {
        RenderObjectDeleter<PLATFORM>::template GetQueue<T>().Push(std::move(it));
    }
}

template <class T, SizeType Sz, renderer::PlatformType PLATFORM>
static inline void SafeRelease(FixedArray<renderer::RenderObjectHandle<T, PLATFORM>, Sz> &&handles)
{
    for (auto &it : handles) {
        RenderObjectDeleter<PLATFORM>::template GetQueue<T>().Push(std::move(it));
    }
}

} // namespace hyperion

#endif