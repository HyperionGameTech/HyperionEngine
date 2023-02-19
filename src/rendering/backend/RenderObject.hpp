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

#include <mutex>
#include <atomic>

namespace hyperion {
namespace renderer {

template <class T>
struct RenderObjectDefinition;

template <class T>
constexpr bool has_render_object_defined = implementation_exists<RenderObjectDefinition<T>>;

template <class T>
static inline Name GetNameForRenderObject()
{
    static_assert(has_render_object_defined<T>, "T must be a render object");

    return RenderObjectDefinition<T>::GetNameForType();
}

template <class T>
class RenderObjectContainer
{
    using IDCreator = v2::IDCreator;

public:
    static constexpr SizeType max_size = RenderObjectDefinition<T>::max_size;

    static_assert(has_render_object_defined<T>, "T is not a render object!");

    struct Instance
    {
        ValueStorage<T> storage;
        AtomicVar<UInt16> ref_count;
        bool has_value;

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
            AssertThrowMsg(HasValue(), "Render object of type %s has no value!", GetNameForRenderObject<T>().LookupString().Data());

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
        const UInt index = IDCreator::ForType<T>().NextID() - 1;

        AssertThrowMsg(
            index < RenderObjectDefinition<T>::max_size,
            "Maximum number of RenderObject type allocated! Maximum: %llu\n",
            RenderObjectDefinition<T>::max_size
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
            IDCreator::ForType<T>().FreeID(index + 1);
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
    HeapArray<Instance, max_size> m_data;
    SizeType m_size;
};

template <class T>
class RenderObjectHandle;

class RenderObjects
{
public:
    template <class T>
    static RenderObjectContainer<T> &GetRenderObjectContainer()
    {
        static_assert(has_render_object_defined<T>, "Not a valid render object");

        static RenderObjectContainer<T> container;

        return container;
    }

    template <class T, class ...Args>
    static RenderObjectHandle<T> Make(Args &&... args)
    {
        auto &container = GetRenderObjectContainer<T>();

        const UInt index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<Args>(args)...
        );

        return RenderObjectHandle<T>::FromIndex(index + 1);
    }
};

template <class T>
class RenderObjectHandle
{
    static_assert(has_render_object_defined<T>, "Not a valid render object");

    UInt index;

    RenderObjectContainer<T> *_container = &RenderObjects::GetRenderObjectContainer<T>();

public:

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
        { return GetNameForRenderObject<T>(); }

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

    template <class ...Args>
    [[nodiscard]] static RenderObjectHandle Construct(Args &&... args)
    {
        return RenderObjects::Make<T>(std::forward<Args>(args)...);
    }
};

} // namespace renderer

#define DEF_RENDER_OBJECT(T, _max_size) \
    namespace renderer { \
    class T; \
    template <> \
    struct RenderObjectDefinition< T > \
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
    using T##Ref = renderer::RenderObjectHandle< renderer::T >

DEF_RENDER_OBJECT(CommandBuffer,       2048);
DEF_RENDER_OBJECT(ShaderProgram,       2048);
DEF_RENDER_OBJECT(GraphicsPipeline,    2048);
DEF_RENDER_OBJECT(GPUBuffer,           65536);
DEF_RENDER_OBJECT(Image,               16384);
DEF_RENDER_OBJECT(ImageView,           65536);
DEF_RENDER_OBJECT(Sampler,             16384);
DEF_RENDER_OBJECT(RaytracingPipeline,  128);
DEF_RENDER_OBJECT(DescriptorSet,       4096);
DEF_RENDER_OBJECT(ComputePipeline,     4096);
DEF_RENDER_OBJECT(Attachment,          4096);
DEF_RENDER_OBJECT(AttachmentUsage,     8192);

#undef DEF_RENDER_OBJECT

using RenderObjects = renderer::RenderObjects;

struct RenderObjectDeleter
{
    static constexpr UInt initial_cycles_remaining = max_frames_in_flight + 1;
    static constexpr SizeType max_queues = 63;

    struct DeletionQueueBase
    {
        TypeID type_id;
        AtomicVar<UInt32> num_items { 0 };
        std::mutex mtx;

        virtual ~DeletionQueueBase() = default;
        virtual void Iterate() = 0;
    };

    template <class T>
    struct DeletionQueue : DeletionQueueBase
    {
        Array<Pair<renderer::RenderObjectHandle<T>, UInt8>> items;
        Queue<renderer::RenderObjectHandle<T>> to_delete;

        DeletionQueue()
        {
            type_id = TypeID::ForType<T>();
        }

        DeletionQueue(const DeletionQueue &other) = delete;
        DeletionQueue &operator=(const DeletionQueue &other) = delete;
        virtual ~DeletionQueue() = default;

        virtual void Iterate() override
        {
            if (!num_items.Get(MemoryOrder::ACQUIRE)) {
                return;
            }
            
            mtx.lock();

            for (auto it = items.Begin(); it != items.End();) {
                if (--it->second == 0) {
                    to_delete.Push(std::move(it->first));

                    it = items.Erase(it);
                } else {
                    ++it;
                }
            }

            num_items.Set(items.Size(), MemoryOrder::RELEASE);

            mtx.unlock();

            while (to_delete.Any()) {
                HYPERION_ASSERT_RESULT(to_delete.Front()->Destroy(v2::GetEngineDevice()));

                to_delete.Pop();
            }
        }

        void Push(renderer::RenderObjectHandle<T> &&handle)
        {
            num_items.Increment(1, MemoryOrder::RELAXED);

            std::lock_guard guard(mtx);

            items.PushBack({ std::move(handle), initial_cycles_remaining });
        }
    };

    template <class T>
    struct DeletionQueueInstance
    {
        DeletionQueue<T> queue;
        UInt16 index;

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

    static FixedArray<DeletionQueueBase *, max_queues + 1> queues;
    static AtomicVar<UInt16> queue_index;
};

template <class T>
static inline void SafeRelease(renderer::RenderObjectHandle<T> &&handle)
{
    RenderObjectDeleter::GetQueue<T>().Push(std::move(handle));
}

template <class T, SizeType Sz>
static inline void SafeRelease(Array<renderer::RenderObjectHandle<T>, Sz> &&handles)
{
    for (auto &it : handles) {
        RenderObjectDeleter::GetQueue<T>().Push(std::move(it));
    }
}

template <class T, SizeType Sz>
static inline void SafeRelease(FixedArray<renderer::RenderObjectHandle<T>, Sz> &&handles)
{
    for (auto &it : handles) {
        RenderObjectDeleter::GetQueue<T>().Push(std::move(it));
    }
}

} // namespace hyperion

#endif