/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashSet.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/Name.hpp>

#include <HashCode.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/Shared.hpp>

#include <mutex>

namespace hyperion {

class Engine;

template <class T>
struct RenderObjectHeader;

class HYP_API RenderObjectContainerBase
{
protected:
    RenderObjectContainerBase(ANSIStringView renderObjectTypeName);

public:
    virtual ~RenderObjectContainerBase() = default;

    virtual void ReleaseIndex(uint32 index) = 0;
};

template <class T>
class RenderObjectContainer final : public MemoryPool<RenderObjectHeader<T>>, public RenderObjectContainerBase
{
public:
    RenderObjectContainer()
        : RenderObjectContainerBase(TypeNameHelper<T, true>::value)
    {
    }

    RenderObjectContainer(const RenderObjectContainer& other) = delete;
    RenderObjectContainer& operator=(const RenderObjectContainer& other) = delete;
    virtual ~RenderObjectContainer() override = default;

    virtual void ReleaseIndex(uint32 index) override
    {
        MemoryPool<RenderObjectHeader<T>>::ReleaseIndex(index);
    }
};

struct RenderObjectHeaderBase
{
    RenderObjectContainerBase* container;
    uint32 index;
    AtomicVar<uint32> refCountStrong;
    AtomicVar<uint32> refCountWeak;
    void (*deleter)(RenderObjectHeaderBase*);

#ifdef HYP_DEBUG_MODE
    Name debugName;
#endif

    RenderObjectHeaderBase()
        : container(nullptr),
          index(~0u),
          refCountStrong(0),
          refCountWeak(0),
          deleter(nullptr)
    {
    }

    ~RenderObjectHeaderBase()
    {
        AssertDebug(!HasValue());
        AssertDebug(refCountStrong.Get(MemoryOrder::ACQUIRE) == 0);
        AssertDebug(refCountWeak.Get(MemoryOrder::ACQUIRE) == 0);
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return index != ~0u;
    }

    HYP_FORCE_INLINE uint32 GetRefCountStrong() const
    {
        return refCountStrong.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE uint32 GetRefCountWeak() const
    {
        return refCountWeak.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE void IncRefStrong()
    {
#ifdef HYP_DEBUG_MODE
        uint32 count = refCountStrong.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
        if (count == UINT32_MAX)
        {
            HYP_FAIL("Ref count overflow!");
        }
#else
        refCountStrong.Increment(1, MemoryOrder::RELEASE);
#endif
    }

    uint32 DecRefStrong()
    {
        AssertDebug(HasValue());
        AssertDebug(GetRefCountStrong() != 0);

        uint16 count;

        if ((count = refCountStrong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
            refCountWeak.Increment(1, MemoryOrder::RELEASE);

            AssertDebug(deleter != nullptr);

            deleter(this);

            if (refCountWeak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) == 1)
            {
                container->ReleaseIndex(index);
                index = ~0u;
            }
        }

        return uint32(count) - 1;
    }

    HYP_FORCE_INLINE void IncRefWeak()
    {
#ifdef HYP_DEBUG_MODE
        uint32 count = refCountWeak.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
        if (count == UINT32_MAX)
        {
            HYP_FAIL("Ref count overflow!");
        }
#else
        refCountWeak.Increment(1, MemoryOrder::RELEASE);
#endif
    }

    uint32 DecRefWeak()
    {
        AssertDebug(HasValue());
        AssertDebug(GetRefCountWeak() != 0);

        uint16 count;

        if ((count = refCountWeak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
            if (refCountStrong.Get(MemoryOrder::ACQUIRE) == 0)
            {
                container->ReleaseIndex(index);
                index = ~0u;
            }
        }

        return uint32(count) - 1;
    }

    HYP_FORCE_INLINE Name GetDebugName() const
    {
#ifdef HYP_DEBUG_MODE
        return debugName;
#else
        return HYP_NAME("<invalid>");
#endif
    }

    HYP_FORCE_INLINE void SetDebugName(Name name)
    {
#ifdef HYP_DEBUG_MODE
        debugName = name;
#endif
    }
};

template <class T>
struct RenderObjectHeader final : RenderObjectHeaderBase
{
    ValueStorage<T> storage;

    RenderObjectHeader()
        : RenderObjectHeaderBase()
    {
        deleter = [](RenderObjectHeaderBase* header)
        {
            static_cast<RenderObjectHeader<T>*>(header)->storage.Destruct();
        };
    }

    RenderObjectHeader(const RenderObjectHeader& other) = delete;
    RenderObjectHeader& operator=(const RenderObjectHeader& other) = delete;
    RenderObjectHeader(RenderObjectHeader&& other) noexcept = delete;
    RenderObjectHeader& operator=(RenderObjectHeader&& other) = delete;
    ~RenderObjectHeader() = default;

    template <class... Args>
    T* Construct(Args&&... args)
    {
        T* ptr = storage.Construct(std::forward<Args>(args)...);

        return ptr;
    }

    HYP_FORCE_INLINE T& Get()
    {
        AssertDebug(HasValue(), "Render object of type {} has no value!", TypeNameHelper<T, true>::value.Data());

        return storage.Get();
    }
};

struct ConstructRenderObjectContext
{
    RenderObjectHeaderBase* header;
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

    static const RenderObjectHandle_Strong& Null();

    RenderObjectHandle_Strong()
        : header(nullptr),
          ptr(nullptr)
    {
    }

    RenderObjectHandle_Strong(RenderObjectHeaderBase* header, T* ptr)
        : header(header),
          ptr(ptr)
    {
        if (header)
        {
            header->IncRefStrong();
        }
    }

    explicit RenderObjectHandle_Strong(RenderObjectHeader<T>* header)
        : header(header),
          ptr(header ? &header->storage.Get() : nullptr)
    {
        if (header)
        {
            header->IncRefStrong();
        }
    }

    RenderObjectHandle_Strong(std::nullptr_t)
        : header(nullptr),
          ptr(nullptr)
    {
    }

    RenderObjectHandle_Strong& operator=(std::nullptr_t)
    {
        if (header)
        {
            auto* _header = header;
            header = nullptr;

            ptr = nullptr;

            _header->DecRefStrong();
        }

        return *this;
    }

    RenderObjectHandle_Strong(const RenderObjectHandle_Strong& other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header)
        {
            header->IncRefStrong();
        }
    }

    RenderObjectHandle_Strong& operator=(const RenderObjectHandle_Strong& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (other.header)
        {
            other.header->IncRefStrong();
        }

        if (header)
        {
            auto* _header = header;
            header = nullptr;

            _header->DecRefStrong();
        }

        header = other.header;
        ptr = other.ptr;

        return *this;
    }

    RenderObjectHandle_Strong(RenderObjectHandle_Strong&& other) noexcept
        : header(other.header),
          ptr(other.ptr)
    {
        other.header = nullptr;
        other.ptr = nullptr;
    }

    RenderObjectHandle_Strong& operator=(RenderObjectHandle_Strong&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (header)
        {
            auto* _header = header;
            header = nullptr;

            _header->DecRefStrong();
        }

        header = other.header;
        ptr = other.ptr;

        other.header = nullptr;
        other.ptr = nullptr;

        return *this;
    }

    ~RenderObjectHandle_Strong()
    {
        if (header)
        {
            auto* _header = header;
            header = nullptr;

            _header->DecRefStrong();
        }
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return ptr;
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        return *ptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const T* obj) const
    {
        return ptr == obj;
    }

    HYP_FORCE_INLINE bool operator!=(const T* obj) const
    {
        return ptr != obj;
    }

    HYP_FORCE_INLINE bool operator==(const RenderObjectHandle_Strong& other) const
    {
        return header == other.header && ptr == other.ptr;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> || std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE bool operator==(const RenderObjectHandle_Strong<Ty>& other) const
    {
        return header == other.header && static_cast<Ty*>(ptr) == other.ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const RenderObjectHandle_Strong& other) const
    {
        return header != other.header || ptr != other.ptr;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> || std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE bool operator!=(const RenderObjectHandle_Strong<Ty>& other) const
    {
        return header != other.header || static_cast<Ty*>(ptr) != other.ptr;
    }

    HYP_FORCE_INLINE bool operator<(const RenderObjectHandle_Strong& other) const
    {
        return header < other.header;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> || std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE bool operator<(const RenderObjectHandle_Strong<Ty>& other) const
    {
        return header < other.header;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return header != nullptr && ptr != nullptr;
    }

    HYP_FORCE_INLINE uint32 GetRefCount() const
    {
        return header ? header->GetRefCountStrong() : 0;
    }

    HYP_FORCE_INLINE T* Get() const&
    {
        return ptr;
    }

    HYP_FORCE_INLINE void Reset()
    {
        if (header)
        {
            auto* _header = header;
            header = nullptr;

            ptr = nullptr;

            _header->DecRefStrong();
        }
    }

    HYP_FORCE_INLINE operator T*() const&
    {
        return Get();
    }

    HYP_FORCE_INLINE operator const T*() const&
    {
        return const_cast<const T*>(Get());
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator RenderObjectHandle_Strong<Ty>() const
    {
        if (header)
        {
            return RenderObjectHandle_Strong<Ty>(header, static_cast<Ty*>(ptr));
        }

        return RenderObjectHandle_Strong<Ty>::unset;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator RenderObjectHandle_Strong<Ty>() const
    {
        if (header)
        {
            return RenderObjectHandle_Strong<Ty>(header, static_cast<Ty*>(ptr));
        }

        return RenderObjectHandle_Strong<Ty>::unset;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(ptr);
    }

    RenderObjectHeaderBase* header;
    T* ptr;
};

template <class T>
const RenderObjectHandle_Strong<T> RenderObjectHandle_Strong<T>::unset = RenderObjectHandle_Strong<T>();

template <class T>
const RenderObjectHandle_Strong<T>& RenderObjectHandle_Strong<T>::Null()
{
    return unset;
}

template <class T>
class RenderObjectHandle_Weak
{
public:
    using Type = T;

    static const RenderObjectHandle_Weak unset;

    static const RenderObjectHandle_Weak& Null();

    RenderObjectHandle_Weak()
        : header(nullptr),
          ptr(nullptr)
    {
    }

    explicit RenderObjectHandle_Weak(RenderObjectHeader<T>* header)
        : header(header),
          ptr(header ? &header->storage.Get() : nullptr)
    {
        if (header)
        {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak(RenderObjectHeaderBase* header, T* ptr)
        : header(header),
          ptr(ptr)
    {
        if (header)
        {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak(std::nullptr_t)
        : header(nullptr),
          ptr(nullptr)
    {
    }

    RenderObjectHandle_Weak& operator=(std::nullptr_t)
    {
        if (header)
        {
            auto* _header = header;
            header = nullptr;

            ptr = nullptr;

            _header->DecRefWeak();
        }

        return *this;
    }

    RenderObjectHandle_Weak(const RenderObjectHandle_Strong<T>& other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header)
        {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak(const RenderObjectHandle_Weak& other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header)
        {
            header->IncRefWeak();
        }
    }

    RenderObjectHandle_Weak& operator=(const RenderObjectHandle_Weak& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (other.header)
        {
            other.header->IncRefWeak();
        }

        if (header)
        {
            auto* _header = header;
            header = nullptr;

            _header->DecRefWeak();
        }

        header = other.header;
        ptr = other.ptr;

        return *this;
    }

    RenderObjectHandle_Weak(RenderObjectHandle_Weak&& other) noexcept
        : header(other.header),
          ptr(other.ptr)
    {
        other.header = nullptr;
        other.ptr = nullptr;
    }

    RenderObjectHandle_Weak& operator=(RenderObjectHandle_Weak&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (header)
        {
            auto* _header = header;
            header = nullptr;

            _header->DecRefWeak();
        }

        ptr = other.ptr;
        other.ptr = nullptr;

        header = other.header;
        other.header = nullptr;

        return *this;
    }

    ~RenderObjectHandle_Weak()
    {
        if (header)
        {
            auto* _header = header;
            header = nullptr;

            _header->DecRefWeak();
        }
    }

    HYP_NODISCARD RenderObjectHandle_Strong<T> Lock() const
    {
        if (!header)
        {
            return RenderObjectHandle_Strong<T>::unset;
        }

        return header->GetRefCountStrong() != 0
            ? RenderObjectHandle_Strong<T>(header, ptr)
            : RenderObjectHandle_Strong<T>::unset;
    }

    HYP_FORCE_INLINE bool operator==(const T* obj) const
    {
        return ptr == obj;
    }

    HYP_FORCE_INLINE bool operator!=(const T* obj) const
    {
        return ptr != obj;
    }

    HYP_FORCE_INLINE bool operator==(const RenderObjectHandle_Weak& other) const
    {
        return header == other.header && ptr != other.ptr;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> || std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE bool operator==(const RenderObjectHandle_Weak<Ty>& other) const
    {
        return header == other.header && static_cast<Ty*>(ptr) != other.ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const RenderObjectHandle_Weak& other) const
    {
        return header != other.header || ptr != other.ptr;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> || std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE bool operator!=(const RenderObjectHandle_Weak<Ty>& other) const
    {
        return header != other.header || static_cast<Ty*>(ptr) != other.ptr;
    }

    HYP_FORCE_INLINE bool operator<(const RenderObjectHandle_Weak& other) const
    {
        return header < other.header;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> || std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE bool operator<(const RenderObjectHandle_Weak<Ty>& other) const
    {
        return header < other.header;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return header != nullptr && ptr != nullptr;
    }

    HYP_FORCE_INLINE void Reset()
    {
        if (header)
        {
            auto* _header = header;
            header = nullptr;

            ptr = nullptr;

            _header->DecRefWeak();
        }
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator RenderObjectHandle_Weak<Ty>() const
    {
        if (header)
        {
            return RenderObjectHandle_Weak<Ty>(header, static_cast<Ty*>(ptr));
        }

        return RenderObjectHandle_Weak<Ty>::unset;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator RenderObjectHandle_Weak<Ty>() const
    {
        if (header)
        {
            return RenderObjectHandle_Weak<Ty>(header, static_cast<Ty*>(ptr));
        }

        return RenderObjectHandle_Weak<Ty>::unset;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(ptr);
    }

    RenderObjectHeaderBase* header;
    T* ptr;
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
        auto* _header = m_header;
        m_header = nullptr;

        _header->DecRefWeak();
    }


#ifdef HYP_DEBUG_MODE
public:
#endif

    HYP_FORCE_INLINE RenderObjectHeaderBase* GetHeader_Internal() const
    {
        return m_header;
    }

public:
    HYP_FORCE_INLINE Name GetDebugName() const
    {
        return m_header->GetDebugName();
    }

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name)
    {
        AssertDebug(m_header != nullptr);

        m_header->SetDebugName(name);
    }
#else
    void SetDebugName(Name)
    {
        /* no implementation */
    }
#endif

private:
    RenderObjectHeaderBase* m_header;
};

template <class T>
class RenderObject : public RenderObjectBase
{
public:
    using Type = T;

    virtual ~RenderObject() override = default;

    HYP_NODISCARD RenderObjectHandle_Strong<T> HandleFromThis() const
    {
        return RenderObjectHandle_Strong<T>(GetHeader_Internal(), static_cast<T*>(const_cast<RenderObject<T>*>(this)));
    }

    HYP_NODISCARD RenderObjectHandle_Weak<T> WeakHandleFromThis() const
    {
        return RenderObjectHandle_Weak<T>(GetHeader_Internal(), static_cast<T*>(const_cast<RenderObject<T>*>(this)));
    }
};

class RenderObjects
{
public:
    template <class T>
    static RenderObjectContainer<T>& GetRenderObjectContainer()
    {
        static RenderObjectContainer<T> container;

        return container;
    }

    template <class T, class... Args>
    static RenderObjectHandle_Strong<T> Make(Args&&... args)
    {
        RenderObjectContainer<T>& container = GetRenderObjectContainer<T>();

        RenderObjectHeader<T>* header;
        uint32 index = container.AcquireIndex(&header);

        header->index = index;
        header->container = &container;

        header->IncRefWeak();

        // This is a bit of a hack, but we need to pass the header to the constructor of RenderObjectBase
        // so HandleFromThis() and WeakHandleFromThis() are usable within the constructor of T and its base classes.
        PushGlobalContext<ConstructRenderObjectContext>(ConstructRenderObjectContext { header });

        header->Construct(std::forward<Args>(args)...);

        AssertDebug(static_cast<T&>(header->storage.Get()).RenderObjectBase::m_header == header);

        return RenderObjectHandle_Strong<T>(header);
    }
};

#define DECLARE_SHARED_GFX_TYPE(T)                                                            \
    class T##Base;                                                                            \
    class Vulkan##T;                                                                          \
                                                                                              \
    using T##Ref = RenderObjectHandle_Strong<T##Base>;                                        \
    using T##WeakRef = RenderObjectHandle_Weak<T##Base>;                                      \
                                                                                              \
    using Vulkan##T##Ref = RenderObjectHandle_Strong<Vulkan##T>;                              \
    using Vulkan##T##WeakRef = RenderObjectHandle_Weak<Vulkan##T>;                            \
                                                                                              \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>>         \
    static inline Vulkan##T* VulkanCastImpl(Base* ptr)                                        \
    {                                                                                         \
        return static_cast<Vulkan##T*>(ptr);                                                  \
    }                                                                                         \
                                                                                              \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>>         \
    static inline const Vulkan##T* VulkanCastImpl(const Base* ptr)                            \
    {                                                                                         \
        return static_cast<const Vulkan##T*>(ptr);                                            \
    }                                                                                         \
                                                                                              \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>>         \
    static inline Vulkan##T##Ref VulkanCastImpl(const RenderObjectHandle_Strong<Base>& ref)   \
    {                                                                                         \
        return Vulkan##T##Ref(ref);                                                           \
    }                                                                                         \
                                                                                              \
    template <class Base, typename = std::enable_if_t<std::is_same_v<Base, T##Base>>>         \
    static inline Vulkan##T##WeakRef VulkanCastImpl(const RenderObjectHandle_Weak<Base>& ref) \
    {                                                                                         \
        return Vulkan##T##WeakRef(ref);                                                       \
    }

#define DECLARE_VULKAN_GFX_TYPE(T)                               \
    class Vulkan##T;                                             \
                                                                 \
    using Vulkan##T##Ref = RenderObjectHandle_Strong<Vulkan##T>; \
    using Vulkan##T##WeakRef = RenderObjectHandle_Weak<Vulkan##T>;

#define VULKAN_CAST(a) VulkanCastImpl(a)

/*! \brief Enqueues a render object to be created with the given args on the render thread, or creates it immediately if already on the render thread.
 *
 *  \param ref The render object to create.
 *  \param args The arguments to pass to the render object's constructor.
 */
template <class RefType, class... Args>
static inline void DeferCreate(RefType ref, Args&&... args)
{
    struct RENDER_COMMAND(CallCreateOnRenderThread) final : RenderCommand
    {
        using ArgsTuple = Tuple<std::decay_t<Args>...>;

        RefType ref;
        ArgsTuple args;

        RENDER_COMMAND(CallCreateOnRenderThread)(RefType&& ref, Args&&... args)
            : ref(std::move(ref)),
              args(std::forward<Args>(args)...)
        {
        }

        virtual ~RENDER_COMMAND(CallCreateOnRenderThread)() override = default;

        virtual RendererResult operator()() override
        {
            return Apply([this]<class... OtherArgs>(OtherArgs&&... args)
                {
                    return ref->Create(std::forward<OtherArgs>(args)...);
                },
                std::move(args));
        }
    };

    if (!ref.IsValid())
    {
        return;
    }

    PUSH_RENDER_COMMAND(CallCreateOnRenderThread, std::move(ref), std::forward<Args>(args)...);
}

#if HYP_VULKAN
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T = T##_VULKAN;                     \
    using T##Ref = T##Ref##_VULKAN;           \
    using T##WeakRef = T##WeakRef##_VULKAN
#elif HYP_WEBGPU
#define DEF_CURRENT_PLATFORM_RENDER_OBJECT(T) \
    using T = T##_WEBGPU;                     \
    using T##Ref = T##Ref##_WEBGPU;           \
    using T##WeakRef = T##WeakRef##_WEBGPU
#endif

#include <rendering/inl/RenderObjectDefinitions.inl>

#undef DEF_RENDER_OBJECT
#undef DEF_RENDER_OBJECT_WITH_BASE_CLASS
#undef DEF_RENDER_OBJECT_NAMED
#undef DEF_RENDER_PLATFORM_OBJECT
#undef DEF_CURRENT_PLATFORM_RENDER_OBJECT

template <class T, class... Args>
static inline RenderObjectHandle_Strong<T> MakeRenderObject(Args&&... args)
{
    return RenderObjects::Make<T>(std::forward<Args>(args)...);
}

struct DeletionQueueBase
{
    TypeId typeId;
    AtomicVar<int32> numItems { 0 };
    std::mutex mtx;

    virtual ~DeletionQueueBase() = default;

    virtual void Iterate() = 0;
    virtual int32 RemoveAllNow(bool force = false) = 0;
};

struct RenderObjectDeleter
{
    static constexpr uint32 initialCyclesRemaining = 4;
    static constexpr SizeType maxQueues = 63;

    static FixedArray<DeletionQueueBase*, maxQueues + 1> s_queues;
    static AtomicVar<uint16> s_queueIndex;

    template <class T>
    struct DeletionQueue : DeletionQueueBase
    {
        using Base = DeletionQueueBase;

        Array<Pair<RenderObjectHandle_Strong<T>, uint8>> items;
        Queue<RenderObjectHandle_Strong<T>> toDelete;

        DeletionQueue()
        {
            Base::typeId = TypeId::ForType<T>();
        }

        DeletionQueue(const DeletionQueue& other) = delete;
        DeletionQueue& operator=(const DeletionQueue& other) = delete;
        virtual ~DeletionQueue() override = default;

        virtual void Iterate() override
        {
            Threads::AssertOnThread(g_renderThread);

            if (Base::numItems.Get(MemoryOrder::ACQUIRE) <= 0)
            {
                return;
            }

            Base::mtx.lock();

            for (auto it = items.Begin(); it != items.End();)
            {
                if (--it->second == 0)
                {
                    toDelete.Push(std::move(it->first));

                    it = items.Erase(it);

                    Base::numItems.Decrement(1, MemoryOrder::RELEASE);
                }
                else
                {
                    ++it;
                }
            }

            Base::mtx.unlock();

            while (toDelete.Any())
            {
                auto object = toDelete.Pop();

                if (object.GetRefCount() > 1)
                {
                    continue;
                }

                HYP_GFX_ASSERT(object->Destroy());
            }
        }

        virtual int32 RemoveAllNow(bool force) override
        {
            Threads::AssertOnThread(g_renderThread);

            if (Base::numItems.Get(MemoryOrder::ACQUIRE) <= 0)
            {
                return 0;
            }

            int32 numDeletedObjects = 0;

            Base::mtx.lock();

            for (auto it = items.Begin(); it != items.End();)
            {
                toDelete.Push(std::move(it->first));

                it = items.Erase(it);

                Base::numItems.Decrement(1, MemoryOrder::RELEASE);

                ++numDeletedObjects;
            }

            Base::mtx.unlock();

            while (toDelete.Any())
            {
                auto object = toDelete.Pop();

                if (object.GetRefCount() > 1)
                {
                    continue;
                }

                HYP_GFX_ASSERT(object->Destroy());
            }

            return numDeletedObjects;
        }

        void Push(RenderObjectHandle_Strong<T>&& handle)
        {
            std::lock_guard guard(Base::mtx);

            items.PushBack({ std::move(handle), initialCyclesRemaining });
            handle = {};

            Base::numItems.Increment(1, MemoryOrder::RELEASE);
        }
    };

    template <class T>
    struct DeletionQueueInstance
    {
        DeletionQueue<T> queue;
        uint16 index;

        DeletionQueueInstance()
        {
            index = s_queueIndex.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

            Assert(index < maxQueues, "Maximum number of deletion queues added");

            s_queues[index] = &queue;
        }

        DeletionQueueInstance(const DeletionQueueInstance& other) = delete;
        DeletionQueueInstance& operator=(const DeletionQueueInstance& other) = delete;
        DeletionQueueInstance(DeletionQueueInstance&& other) noexcept = delete;
        DeletionQueueInstance& operator=(DeletionQueueInstance&& other) noexcept = delete;

        ~DeletionQueueInstance()
        {
            s_queues[index] = nullptr;
        }
    };

    template <class T>
    static DeletionQueue<T>& GetQueue()
    {
        static DeletionQueueInstance<T> instance;

        return instance.queue;
    }

    static void Initialize();
    static void Iterate();
    static void RemoveAllNow(bool force = false);
};

template <class T>
static inline void SafeRelease(RenderObjectHandle_Strong<T>&& handle)
{
    if (!handle.IsValid())
    {
        return;
    }

    static_assert(implementationExists<T>, "T not a defined type - cannot use SafeRelease()");

    RenderObjectDeleter::GetQueue<typename T::Type>().Push(std::move(handle));
}

template <class T, class AllocatorType>
static inline void SafeRelease(Array<RenderObjectHandle_Strong<T>, AllocatorType>&& handles)
{
    static_assert(implementationExists<T>, "T not a defined type - cannot use SafeRelease()");

    for (auto& it : handles)
    {
        if (!it.IsValid())
        {
            continue;
        }

        RenderObjectDeleter::GetQueue<typename T::Type>().Push(std::move(it));
    }

    handles.Clear();
}

template <class T, SizeType Sz>
static inline void SafeRelease(FixedArray<RenderObjectHandle_Strong<T>, Sz>&& handles)
{
    static_assert(implementationExists<T>, "T not a defined type - cannot use SafeRelease()");

    for (auto& it : handles)
    {
        if (!it.IsValid())
        {
            continue;
        }

        RenderObjectDeleter::GetQueue<typename T::Type>().Push(std::move(it));
    }
}

template <class T, auto KeyBy>
static inline void SafeRelease(HashSet<RenderObjectHandle_Strong<T>, KeyBy>&& handles)
{
    static_assert(implementationExists<T>, "T not a defined type - cannot use SafeRelease()");

    for (auto& it : handles)
    {
        if (!it.IsValid())
        {
            continue;
        }

        RenderObjectDeleter::GetQueue<typename T::Type>().Push(std::move(it));
    }

    handles.Clear();
}

} // namespace hyperion
