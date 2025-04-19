/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_RENDER_OBJECT_HPP
#define HYPERION_BACKEND_RENDERER_RENDER_OBJECT_HPP

#include <core/Defines.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/HeapArray.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/Name.hpp>
#include <core/IDGenerator.hpp>

#include <HashCode.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>

#include <mutex>

namespace hyperion {

class Engine;

namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

} // namespace platform

template <class T>
struct RenderObjectHeader;

class HYP_API RenderObjectContainerBase
{
protected:
    RenderObjectContainerBase(ANSIStringView render_object_type_name);
    virtual ~RenderObjectContainerBase() = default;

public:
    HYP_FORCE_INLINE ANSIStringView GetRenderObjectTypeName() const
        { return m_render_object_type_name; }

    virtual void ReleaseIndex(uint32 index) = 0;

protected:
    ANSIStringView  m_render_object_type_name;
    SizeType        m_size;
};

template <class T>
class RenderObjectContainer final : public MemoryPool<RenderObjectHeader<T>>, public RenderObjectContainerBase
{
public:
    RenderObjectContainer()
        : RenderObjectContainerBase(TypeNameHelper<T, true>::value)
    {
    }

    RenderObjectContainer(const RenderObjectContainer &other) = delete;
    RenderObjectContainer &operator=(const RenderObjectContainer &other) = delete;

    virtual ~RenderObjectContainer() override = default;

    virtual void ReleaseIndex(uint32 index) override
    {
        MemoryPool<RenderObjectHeader<T>>::ReleaseIndex(index);
    }
};

HYP_DISABLE_OPTIMIZATION;
struct RenderObjectHeaderBase
{
    RenderObjectContainerBase   *container;
    uint32                      index;
    AtomicVar<uint16>           ref_count_strong;
    AtomicVar<uint16>           ref_count_weak;
    void                        (*deleter)(RenderObjectHeaderBase *);

    ~RenderObjectHeaderBase()
    {
        AssertDebug(!HasValue());
        AssertDebug(ref_count_strong.Get(MemoryOrder::ACQUIRE) == 0);
        AssertDebug(ref_count_weak.Get(MemoryOrder::ACQUIRE) == 0);
    }

    HYP_FORCE_INLINE bool HasValue() const
        { return index != ~0u; }

    HYP_FORCE_INLINE uint16 GetRefCountStrong() const
        { return ref_count_strong.Get(MemoryOrder::ACQUIRE); }

    HYP_FORCE_INLINE uint16 GetRefCountWeak() const
        { return ref_count_weak.Get(MemoryOrder::ACQUIRE); }

    HYP_FORCE_INLINE void IncRefStrong()
        { ref_count_strong.Increment(1, MemoryOrder::RELAXED); }

    uint32 DecRefStrong()
    {
        AssertDebug(GetRefCountStrong() != 0);
        AssertDebug(HasValue());

        uint16 count;

        if ((count = ref_count_strong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1) {
            ref_count_weak.Increment(1, MemoryOrder::RELEASE);

            deleter(this);

            if (ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) == 1) {
                container->ReleaseIndex(index);
                index = ~0u;
            }
        }

        return uint32(count) - 1;
    }

    HYP_FORCE_INLINE void IncRefWeak()
        { ref_count_weak.Increment(1, MemoryOrder::RELAXED); }

    uint32 DecRefWeak()
    {
        AssertDebug(GetRefCountWeak() != 0);
        AssertDebug(HasValue());

        uint16 count;
        
        if ((count = ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1) {
            if (ref_count_strong.Get(MemoryOrder::ACQUIRE) == 0) {
                container->ReleaseIndex(index);
                index = ~0u;
            }
        }

        return uint32(count) - 1;
    }
};
HYP_ENABLE_OPTIMIZATION;

template <class T>
struct RenderObjectHeader final : RenderObjectHeaderBase
{
    ValueStorage<T>             storage;

    RenderObjectHeader()
    {
        ref_count_strong.Set(0, MemoryOrder::RELAXED);
        ref_count_weak.Set(0, MemoryOrder::RELAXED);
        index = ~0u;
        container = nullptr;
        deleter = [](RenderObjectHeaderBase *header)
        {
            static_cast<RenderObjectHeader<T> *>(header)->storage.Destruct();
        };
    }

    RenderObjectHeader(const RenderObjectHeader &other)             = delete;
    RenderObjectHeader &operator=(const RenderObjectHeader &other)  = delete;
    RenderObjectHeader(RenderObjectHeader &&other) noexcept         = delete;
    RenderObjectHeader &operator=(RenderObjectHeader &&other)       = delete;
    ~RenderObjectHeader()                                           = default;

    template <class ...Args>
    T *Construct(Args &&... args)
    {
        T *ptr = storage.Construct(std::forward<Args>(args)...);

        return ptr;
    }

    HYP_FORCE_INLINE T &Get()
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(HasValue(), "Render object of type %s has no value!", TypeNameHelper<T, true>::value.Data());
#endif

        return storage.Get();
    }
};

struct ConstructRenderObjectContext
{
    RenderObjectHeaderBase  *header;
};

template <class T>
class RenderObjectHandle_Strong;

template <class T>
class RenderObjectHandle_Weak;

template <class T>
class RenderObjectHandle_Strong
{
public:
    using Type = T;

    static const RenderObjectHandle_Strong unset;

    static const RenderObjectHandle_Strong &Null();

    RenderObjectHandle_Strong()
        : header(nullptr),
          ptr(nullptr)
    {
    }
    
    RenderObjectHandle_Strong(RenderObjectHeaderBase *header, T *ptr)
        : header(header),
          ptr(ptr)
    {
        if (header) {
            header->IncRefStrong();
        }
    }

    explicit RenderObjectHandle_Strong(RenderObjectHeader<T> *header)
        : header(header),
          ptr(header ? &header->storage.Get() : nullptr)
    {
        if (header) {
            header->IncRefStrong();
        }
    }

    RenderObjectHandle_Strong(std::nullptr_t)
        : header(nullptr),
          ptr(nullptr)
    {
    }

    RenderObjectHandle_Strong &operator=(std::nullptr_t)
    {
        if (header) {
            header->DecRefStrong();
        }

        header = nullptr;
        ptr = nullptr;

        return *this;
    }

    RenderObjectHandle_Strong(const RenderObjectHandle_Strong &other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header) {
            header->IncRefStrong();
        }
    }

    RenderObjectHandle_Strong &operator=(const RenderObjectHandle_Strong &other)
    {
        if (this == &other || header == other.header) {
            return *this;
        }

        if (header) {
            header->DecRefStrong();
        }

        header = other.header;
        ptr = other.ptr;

        if (header) {
            header->IncRefStrong();
        }

        return *this;
    }

    RenderObjectHandle_Strong(RenderObjectHandle_Strong &&other) noexcept
        : header(other.header),
          ptr(other.ptr)
    {
        other.header = nullptr;
        other.ptr = nullptr;
    }

    RenderObjectHandle_Strong &operator=(RenderObjectHandle_Strong &&other) noexcept
    {
        if (this == &other || header == other.header) {
            return *this;
        }

        if (header) {
            header->DecRefStrong();
        }

        header = other.header;
        ptr = other.ptr;

        other.header = nullptr;
        other.ptr = nullptr;

        return *this;
    }

    ~RenderObjectHandle_Strong()
    {
        if (header) {
            header->DecRefStrong();
        }
    }

    HYP_FORCE_INLINE T *operator->() const
        { return ptr; }

    HYP_FORCE_INLINE T &operator*() const
        { return *ptr; }

    HYP_FORCE_INLINE bool operator!() const
        { return !IsValid(); }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return !IsValid(); }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return IsValid(); }

    HYP_FORCE_INLINE bool operator==(const T *obj) const
        { return ptr == obj; }

    HYP_FORCE_INLINE bool operator!=(const T *obj) const
        { return ptr != obj; }

    HYP_FORCE_INLINE bool operator==(const RenderObjectHandle_Strong &other) const
        { return header == other.header && ptr == other.ptr; }

    HYP_FORCE_INLINE bool operator!=(const RenderObjectHandle_Strong &other) const
        { return header != other.header || ptr != other.ptr; }

    HYP_FORCE_INLINE bool operator<(const RenderObjectHandle_Strong &other) const
        { return header < other.header; }

    HYP_FORCE_INLINE bool IsValid() const
        { return header != nullptr && ptr != nullptr; }

    HYP_FORCE_INLINE uint16 GetRefCount() const
        { return header ? header->GetRefCountStrong() : 0; }

    HYP_FORCE_INLINE T *Get() const &
        { return ptr; }

    HYP_FORCE_INLINE void Reset()
    {
        if (header) {
            header->DecRefStrong();
        }

        header = nullptr;
        ptr = nullptr;
    }

    HYP_FORCE_INLINE operator T *() const &
        { return Get(); }

    HYP_FORCE_INLINE operator const T *() const &
        { return const_cast<const T *>(Get()); }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator RenderObjectHandle_Strong<Ty>() const
    {
        if (header) {
            return RenderObjectHandle_Strong<Ty>(header, static_cast<Ty *>(ptr));
        }

        return RenderObjectHandle_Strong<Ty>::unset;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_base_of_v<T, Ty>, int> = 0>
    HYP_FORCE_INLINE operator RenderObjectHandle_Strong<Ty>() const
    {
        if (header) {
            return RenderObjectHandle_Strong<Ty>(header, static_cast<Ty *>(ptr));
        }

        return RenderObjectHandle_Strong<Ty>::unset;
    }

    HYP_FORCE_INLINE void SetName(Name name)
    {
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return { };
    }

    RenderObjectHeaderBase  *header;
    T                       *ptr;
};

template <class T>
const RenderObjectHandle_Strong<T> RenderObjectHandle_Strong<T>::unset = RenderObjectHandle_Strong<T>();

template <class T>
const RenderObjectHandle_Strong<T> &RenderObjectHandle_Strong<T>::Null()
{
    return unset;
}

template <class T>
class RenderObjectHandle_Weak
{
public:
    using Type = T;
    
    static const RenderObjectHandle_Weak unset;

    static const RenderObjectHandle_Weak &Null();

    RenderObjectHandle_Weak()
        : header(nullptr),
          ptr(nullptr)
    {
    }

    explicit RenderObjectHandle_Weak(RenderObjectHeader<T> *header)
        : header(header),
          ptr(header ? &header->storage.Get() : nullptr)
    {
        if (header) {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak(RenderObjectHeaderBase *header, T *ptr)
        : header(header),
          ptr(ptr)
    {
        if (header) {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak(std::nullptr_t)
        : header(nullptr),
          ptr(nullptr)
    {
    }

    RenderObjectHandle_Weak &operator=(std::nullptr_t)
    {
        if (header) {
            header->DecRefWeak();
        }

        header = nullptr;
        ptr = nullptr;

        return *this;
    }

    RenderObjectHandle_Weak(const RenderObjectHandle_Strong<T> &other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header) {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak(const RenderObjectHandle_Weak &other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header) {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak &operator=(const RenderObjectHandle_Weak &other)
    {
        if (this == &other || header == other.header) {
            return *this;
        }

        if (header) {
            header->DecRefWeak();
        }

        header = other.header;
        ptr = other.ptr;

        if (header) {
            header->IncRefWeak();
        }

        return *this;
    }

    RenderObjectHandle_Weak(RenderObjectHandle_Weak &&other) noexcept
        : header(other.header),
          ptr(other.ptr)
    {
        other.header = nullptr;
        other.ptr = nullptr;
    }

    RenderObjectHandle_Weak &operator=(RenderObjectHandle_Weak &&other) noexcept
    {
        if (this == &other || header == other.header) {
            return *this;
        }

        if (header) {
            header->DecRefWeak();
        }

        ptr = other.ptr;
        other.ptr = nullptr;

        header = other.header;
        other.header = nullptr;

        return *this;
    }

    ~RenderObjectHandle_Weak()
    {
        if (header) {
            header->DecRefWeak();
        }
    }

    HYP_NODISCARD RenderObjectHandle_Strong<T> Lock() const
    {
        if (!header) {
            return RenderObjectHandle_Strong<T>::unset;
        }

        return header->GetRefCountStrong() != 0
            ? RenderObjectHandle_Strong<T>(header, ptr)
            : RenderObjectHandle_Strong<T>::unset;
    }

    HYP_FORCE_INLINE bool operator==(const RenderObjectHandle_Weak &other) const
        { return header == other.header && ptr != other.ptr; }

    HYP_FORCE_INLINE bool operator!=(const RenderObjectHandle_Weak &other) const
        { return header != other.header || ptr != other.ptr; }

    HYP_FORCE_INLINE bool operator<(const RenderObjectHandle_Weak &other) const
        { return header < other.header; }

    HYP_FORCE_INLINE bool IsValid() const
        { return header != nullptr && ptr != nullptr; }

    HYP_FORCE_INLINE void Reset()
    {
        if (header) {
            header->DecRefWeak();
        }

        header = nullptr;
        ptr = nullptr;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE operator RenderObjectHandle_Weak<Ty>() const
    {
        if (header) {
            return RenderObjectHandle_Weak<Ty>(header, static_cast<Ty *>(ptr));
        }

        return RenderObjectHandle_Weak<Ty>::unset;
    }

    RenderObjectHeaderBase  *header;
    T                       *ptr;
};

template <class T>
const RenderObjectHandle_Weak<T> RenderObjectHandle_Weak<T>::unset = RenderObjectHandle_Weak<T>();

class RenderObjectBase
{
protected:
    friend class RenderObjects;

    RenderObjectBase()
        : m_header(PopGlobalContext<ConstructRenderObjectContext>().header)
    {
    }

    virtual ~RenderObjectBase()
    {
        AssertDebug(m_header != nullptr);
        m_header->DecRefWeak();
    }

    HYP_FORCE_INLINE RenderObjectHeaderBase *GetHeader_Internal() const
        { return m_header; }
    
private:
    RenderObjectHeaderBase  *m_header;
};

template <class T>
class RenderObject : public RenderObjectBase
{
public:
    using Type = T;

    virtual ~RenderObject() override = default;

    HYP_NODISCARD RenderObjectHandle_Strong<T> HandleFromThis()
    {
        return RenderObjectHandle_Strong<T>(GetHeader_Internal(), static_cast<T *>(this));
    }

    HYP_NODISCARD RenderObjectHandle_Weak<T> WeakHandleFromThis()
    {
        return RenderObjectHandle_Weak<T>(GetHeader_Internal(), static_cast<T *>(this));
    }
};

class RenderObjects
{
public:
    template <class T>
    static RenderObjectContainer<T> &GetRenderObjectContainer()
    {
        static RenderObjectContainer<T> container;

        return container;
    }

    template <class T, class... Args>
    static RenderObjectHandle_Strong<T> Make(Args &&... args)
    {
        RenderObjectContainer<T> &container = GetRenderObjectContainer<T>();

        RenderObjectHeader<T> *header;
        uint32 index = container.AcquireIndex(&header);

        header->index = index;
        header->container = &container;

        header->IncRefWeak();

        // This is a bit of a hack, but we need to pass the header to the constructor of RenderObjectBase
        // so HandleFromThis() and WeakHandleFromThis() are usable within the constructor of T and its base classes.
        PushGlobalContext<ConstructRenderObjectContext>(ConstructRenderObjectContext { header });

        header->Construct(std::forward<Args>(args)...);

        AssertDebug(static_cast<T &>(header->storage.Get()).RenderObjectBase::m_header == header);
        
        return RenderObjectHandle_Strong<T>(header);
    }
};


} // namespace renderer

#define DEF_RENDER_PLATFORM_OBJECT_(_platform, T, _max_size) \
    namespace renderer { \
    using T##_##_platform = platform::T<renderer::Platform::_platform>; \
    } \
    using T##_##_platform = renderer::T##_##_platform; \
    using T##Ref##_##_platform = renderer::RenderObjectHandle_Strong< renderer::T##_##_platform >; \
    using T##WeakRef##_##_platform = renderer::RenderObjectHandle_Weak< renderer::T##_##_platform >; \


#define DEF_RENDER_PLATFORM_OBJECT2_(_platform, T, _max_size) \
    namespace renderer { \
    class _platform##T; \
    } \
    using _platform ## T = renderer:: _platform ## T; \
    using _platform ## T ## Ref = renderer::RenderObjectHandle_Strong< renderer:: _platform ## T >;

#define DEF_RENDER_PLATFORM_OBJECT_NAMED_(_platform, name, T, _max_size) \
    namespace renderer { \
    using name##_##_platform = platform::T<renderer::Platform::_platform>; \
    } \
    using name##_##_platform = renderer::name##_##_platform; \
    using name##Ref##_##_platform = renderer::RenderObjectHandle_Strong< renderer::name##_##_platform >; \
    using name##WeakRef##_##_platform = renderer::RenderObjectHandle_Weak< renderer::name##_##_platform >; \

#define DEF_RENDER_PLATFORM_OBJECT(T, _max_size) \
    namespace renderer { \
    namespace platform { \
    template <PlatformType PLATFORM> \
    class T; \
    } \
    } \
    DEF_RENDER_PLATFORM_OBJECT_(VULKAN, T, _max_size) \
    DEF_RENDER_PLATFORM_OBJECT_(WEBGPU, T, _max_size) \
    namespace renderer { \
    namespace platform { \
    template <PlatformType _platform> \
    using T##Ref = renderer::RenderObjectHandle_Strong< T<_platform> >; \
    template <PlatformType _platform> \
    using T##WeakRef = renderer::RenderObjectHandle_Weak< T<_platform> >; \
    } \
    } \

#define DEF_RENDER_PLATFORM_OBJECT_WITH_BASE_CLASS(T, _max_size) \
    namespace renderer { \
    class T##Base; \
    namespace platform { \
    template <PlatformType PLATFORM> \
    class T; \
    } \
    } \
    DEF_RENDER_PLATFORM_OBJECT_(VULKAN, T, _max_size) \
    DEF_RENDER_PLATFORM_OBJECT_(WEBGPU, T, _max_size) \
    namespace renderer { \
    namespace platform { \
    template <PlatformType _platform> \
    using T##Ref = renderer::RenderObjectHandle_Strong< T<_platform> >; \
    template <PlatformType _platform> \
    using T##WeakRef = renderer::RenderObjectHandle_Weak< T<_platform> >; \
    } \
    } \
    using renderer::T##Base;

#define DEF_RENDER_PLATFORM_OBJECT_WITH_BASE_CLASS2(T, _max_size) \
    namespace renderer { \
    class T##Base; \
    } \
    DEF_RENDER_PLATFORM_OBJECT2_(Vulkan, T, _max_size) \
    DEF_RENDER_PLATFORM_OBJECT2_(WGPU, T, _max_size) \
    using renderer::T##Base; \
    using T##Ref = renderer::RenderObjectHandle_Strong< T##Base >; \
    using T##WeakRef = renderer::RenderObjectHandle_Weak< T##Base >; \

#define DEF_RENDER_PLATFORM_OBJECT_NAMED(name, T, _max_size) \
    namespace renderer { \
    namespace platform { \
    template <PlatformType PLATFORM> \
    class T; \
    } \
    } \
    DEF_RENDER_PLATFORM_OBJECT_NAMED_(VULKAN, name, T, _max_size) \
    DEF_RENDER_PLATFORM_OBJECT_NAMED_(WEBGPU, name, T, _max_size) \
    namespace renderer { \
    namespace platform { \
    template <PlatformType _platform> \
    using name##Ref = renderer::RenderObjectHandle_Strong< T<_platform> >; \
    template <PlatformType _platform> \
    using name##WeakRef = renderer::RenderObjectHandle_Weak< T<_platform> >; \
    } \
    } \

/*! \brief Enqueues a render object to be created with the given args on the render thread, or creates it immediately if already on the render thread.
 *
 *  \param ref The render object to create.
 *  \param args The arguments to pass to the render object's constructor.
 */
template <class RefType, class ... Args>
static inline void DeferCreate(RefType ref, Args &&... args)
{
    struct RENDER_COMMAND(CreateRenderObject) : renderer::RenderCommand
    {
        using ArgsTuple = Tuple<std::decay_t<Args>...>;

        RefType     ref;
        ArgsTuple   args;

        RENDER_COMMAND(CreateRenderObject)(RefType &&ref, Args &&... args)
            : ref(std::move(ref)),
              args(std::forward<Args>(args)...)
        {
        }

        virtual ~RENDER_COMMAND(CreateRenderObject)() override = default;

        virtual RendererResult operator()() override
        {
            return Apply([this]<class... OtherArgs>(OtherArgs &&... args)
            {
                return ref->Create(std::forward<OtherArgs>(args)...);
            }, std::move(args));
        }
    };

    if (!ref.IsValid()) {
        return;
    }

    PUSH_RENDER_COMMAND(CreateRenderObject, std::move(ref), std::forward<Args>(args)...);
}

#if HYP_VULKAN
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T = T##_VULKAN; \
    using T##Ref = T##Ref##_VULKAN; \
    using T##WeakRef = T##WeakRef##_VULKAN
#elif HYP_WEBGPU
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T = T##_WEBGPU; \
    using T##Ref = T##Ref##_WEBGPU; \
    using T##WeakRef = T##WeakRef##_WEBGPU
#endif

#include <rendering/backend/inl/RenderObjectDefinitions.inl>

#undef DEF_RENDER_OBJECT
#undef DEF_RENDER_OBJECT_WITH_BASE_CLASS
#undef DEF_RENDER_OBJECT_NAMED
#undef DEF_RENDER_PLATFORM_OBJECT
#undef DEF_CURRENT_PLATFORM_RENDER_OBJECT

template <class T, class ... Args>
static inline renderer::RenderObjectHandle_Strong<T> MakeRenderObject(Args &&... args)
    { return renderer::RenderObjects::Make<T>(std::forward<Args>(args)...); }

struct DeletionQueueBase
{
    TypeID              type_id;
    AtomicVar<int32>    num_items { 0 };
    std::mutex          mtx;

    virtual ~DeletionQueueBase() = default;

    virtual void Iterate() = 0;
    virtual int32 RemoveAllNow(bool force = false) = 0;
};

template <renderer::PlatformType PLATFORM>
struct RenderObjectDeleter
{
    static constexpr uint32     initial_cycles_remaining = max_frames_in_flight + 1;
    static constexpr SizeType   max_queues = 63;

    static FixedArray<DeletionQueueBase *, max_queues + 1>  queues;
    static AtomicVar<uint16>                                queue_index;

    template <class T>
    struct DeletionQueue : DeletionQueueBase
    {
        using Base = DeletionQueueBase;

        static constexpr uint32 initial_cycles_remaining = max_frames_in_flight + 1;

        Array<Pair<renderer::RenderObjectHandle_Strong<T>, uint8>>  items;
        Queue<renderer::RenderObjectHandle_Strong<T>>               to_delete;

        DeletionQueue()
        {
            Base::type_id = TypeID::ForType<T>();
        }

        DeletionQueue(const DeletionQueue &other)               = delete;
        DeletionQueue &operator=(const DeletionQueue &other)    = delete;
        virtual ~DeletionQueue() override                       = default;

        virtual void Iterate() override
        {
            Threads::AssertOnThread(g_render_thread);

            if (Base::num_items.Get(MemoryOrder::ACQUIRE) <= 0) {
                return;
            }
            
            Base::mtx.lock();

            for (auto it = items.Begin(); it != items.End();) {
                if (--it->second == 0) {
                    to_delete.Push(std::move(it->first));

                    it = items.Erase(it);

                    Base::num_items.Decrement(1, MemoryOrder::RELEASE);
                } else {
                    ++it;
                }
            }

            Base::mtx.unlock();

            while (to_delete.Any()) {
                auto object = to_delete.Pop();

                if (object.GetRefCount() > 1) {
                    continue;
                }
                
                HYPERION_ASSERT_RESULT(object->Destroy());
            }
        }

        virtual int32 RemoveAllNow(bool force) override
        {
            Threads::AssertOnThread(g_render_thread);

            if (Base::num_items.Get(MemoryOrder::ACQUIRE) <= 0) {
                return 0;
            }

            int32 num_deleted_objects = 0;

            Base::mtx.lock();

            for (auto it = items.Begin(); it != items.End();) {
                to_delete.Push(std::move(it->first));

                it = items.Erase(it);

                Base::num_items.Decrement(1, MemoryOrder::RELEASE);

                ++num_deleted_objects;
            }

            Base::mtx.unlock();

            while (to_delete.Any()) {
                auto object = to_delete.Pop();

                if (object.GetRefCount() > 1) {
                    continue;
                }
                
                HYPERION_ASSERT_RESULT(object->Destroy());
            }

            return num_deleted_objects;
        }

        void Push(renderer::RenderObjectHandle_Strong<T> &&handle)
        {
            std::lock_guard guard(Base::mtx);

            items.PushBack({ std::move(handle), initial_cycles_remaining });
            
            Base::num_items.Increment(1, MemoryOrder::RELEASE);
        }
    };

    template <class T>
    struct DeletionQueueInstance
    {
        DeletionQueue<T>    queue;
        uint16              index;

        DeletionQueueInstance()
        {
            index = queue_index.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

            AssertThrowMsg(index < max_queues, "Maximum number of deletion queues added");

            queues[index] = &queue;
        }

        DeletionQueueInstance(const DeletionQueueInstance &other)                   = delete;
        DeletionQueueInstance &operator=(const DeletionQueueInstance &other)        = delete;
        DeletionQueueInstance(DeletionQueueInstance &&other) noexcept               = delete;
        DeletionQueueInstance &operator=(DeletionQueueInstance &&other) noexcept    = delete;

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

    static void Initialize();
    static void Iterate();
    static void RemoveAllNow(bool force = false);
};

template <renderer::PlatformType PLATFORM>
FixedArray<DeletionQueueBase *, RenderObjectDeleter<PLATFORM>::max_queues + 1> RenderObjectDeleter<PLATFORM>::queues = { };

template <renderer::PlatformType PLATFORM>
AtomicVar<uint16> RenderObjectDeleter<PLATFORM>::queue_index = { 0 };

template <class T>
static inline void SafeRelease(renderer::RenderObjectHandle_Strong<T> &&handle)
{
    if (!handle.IsValid()) {
        return;
    }

    RenderObjectDeleter<renderer::Platform::CURRENT>::template GetQueue<T>().Push(std::move(handle));
}

template <class T, class AllocatorType>
static inline void SafeRelease(Array<renderer::RenderObjectHandle_Strong<T>, AllocatorType> &&handles)
{
    for (auto &it : handles) {
        if (!it.IsValid()) {
            continue;
        }

        RenderObjectDeleter<renderer::Platform::CURRENT>::template GetQueue<T>().Push(std::move(it));
    }
}

template <class T, SizeType Sz>
static inline void SafeRelease(FixedArray<renderer::RenderObjectHandle_Strong<T>, Sz> &&handles)
{
    for (auto &it : handles) {
        if (!it.IsValid()) {
            continue;
        }

        RenderObjectDeleter<renderer::Platform::CURRENT>::template GetQueue<T>().Push(std::move(it));
    }
}

} // namespace hyperion

#endif