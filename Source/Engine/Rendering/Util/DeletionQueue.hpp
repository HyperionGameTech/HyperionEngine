/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/List.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Rendering/RenderResult.hpp>

#include <type_traits>

#ifdef HYP_DX12
#include <wrl.h> // For Microsoft::WRL::ComPtr
#endif           // HYP_DX12

namespace Hyperion {

template <class T>
class DeletionQueueElem;

template <>
class DeletionQueueElem<Handle<ObjectBase>>
{
protected:
    ENGINE_API explicit DeletionQueueElem(ObjectBase* ptr);

public:
    ENGINE_API explicit DeletionQueueElem(Handle<ObjectBase>&& handle)
        : DeletionQueueElem(handle.ptr)
    {
        handle.ptr = nullptr; // unset so DecRefStrong() doesn't get called on Handle destruction.
    }

    static void Destroy(void* ptr)
    {
        static_cast<DeletionQueueElem*>(ptr)->DestroyObject();
    }

private:
    void DestroyObject();

    ObjectBase* ptr;
};

template <class T>
class DeletionQueueElem<Handle<T>> final : public DeletionQueueElem<Handle<ObjectBase>>
{
public:
    ENGINE_API DeletionQueueElem(Handle<T>&& handle)
        : DeletionQueueElem<Handle<ObjectBase>>(handle.ptr)
    {
        handle.ptr = nullptr;
    }
};

template <class T>
class DeletionQueueElem<T*>
{
public:
    explicit DeletionQueueElem(T* ptr)
        : ptr(ptr)
    {
    }

    static void Destroy(void* ptr)
    {
        static_cast<DeletionQueueElem*>(ptr)->DestroyObject();
    }

private:
    void DestroyObject()
    {
        if (!ptr)
            return;

        if constexpr (IsObjectV<T>)
        {
            if (ptr->GetObjectHeader_Internal() != nullptr)
            {
                ptr->Release();
                ptr = nullptr;

                return;
            }
        }

        delete ptr;
        ptr = nullptr;
    }

    T* ptr;
};

#ifdef HYP_DX12

using Microsoft::WRL::ComPtr;

template <class T>
class DeletionQueueElem<ComPtr<T>>
{
public:
    explicit DeletionQueueElem(ComPtr<T>&& ptr)
        : ptr(ptr.Detach())
    {
    }

    static void Destroy(void* ptr)
    {
        static_cast<DeletionQueueElem*>(ptr)->DestroyObject();
    }

private:
    void DestroyObject()
    {
        if (!ptr)
        {
            return;
        }

        ptr->Release();
        ptr = nullptr;
    }

    T* ptr;
};

#endif // HYP_DX12

class ENGINE_API DeletionQueue
{
public:
    struct EntryHeader
    {
        uint32 offset = 0;
        uint32 size = 0;
        uint32 fc = 0; // frame counter when the entry was added

        void (*moveFn)(void*, void*) = nullptr;
        void (*destructFn)(void*) = nullptr;
    };

    class EntryListBase
    {
    public:
        EntryListBase()
            : EntryListBase(~0u)
        {
        }

        explicit EntryListBase(uint32 desiredIdx)
            : currHeaders(&headers[0]),
              bufferPos(0),
              desiredIdx(desiredIdx)
        {
        }

        EntryListBase(const EntryListBase&) = delete;
        EntryListBase& operator=(const EntryListBase&) = delete;

        EntryListBase(EntryListBase&&) noexcept = delete;
        EntryListBase& operator=(EntryListBase&&) noexcept = delete;

        virtual ~EntryListBase() = default;

        void SwapHeaderBuffers()
        {
            currHeaders = (currHeaders == &headers[0]) ? &headers[1] : &headers[0];
        }

        virtual void* Alloc(size_t size, size_t alignment, EntryHeader& outHeader) = 0;
        virtual void ResizeBuffer(size_t newMinSize) = 0;

        void Push(const EntryHeader& header)
        {
            AssertDebug(currHeaders == &headers[0] || currHeaders == &headers[1]);

            currHeaders->PushBack(header);
        }

        // double-buffered to allow adding new entries while iterating.
        // we only actually iterate from headers[0] and move the entries that were added to headers[1] to headers[0] after iterating.
        Array<EntryHeader> headers[2];

        // current headers to iterate over (changed by calling SwapHeaderBuffers())
        Array<EntryHeader>* currHeaders;
        size_t bufferPos;
        uint32 desiredIdx; // only for temp entry lists when we request a specific index!
    };

    template <class AllocatorType>
    class EntryList final : public EntryListBase
    {
    public:
        void* raw;
        size_t bufferSize;

        EntryList()
            : EntryList(~0u)
        {
        }

        explicit EntryList(uint32 desiredIdx)
            : EntryListBase(desiredIdx),
              raw(nullptr),
              bufferSize(0)
        {
        }

        EntryList(const EntryList&) = delete;
        EntryList& operator=(const EntryList&) = delete;

        EntryList(EntryList&&) noexcept = delete;
        EntryList& operator=(EntryList&&) noexcept = delete;

        ~EntryList() override
        {
            Free();
        }

        virtual void* Alloc(size_t size, size_t alignment, EntryHeader& outHeader) override
        {
            AssertDebug(alignment <= 16);

            const size_t alignedOffset = ByteUtil::AlignAs(bufferPos, alignment);

            if (bufferSize < alignedOffset + size)
            {
                ResizeBuffer(alignedOffset + size);
            }

            UIntPtr ptr = reinterpret_cast<UIntPtr>(raw) + alignedOffset;

            bufferPos = alignedOffset + size;

            outHeader = {};
            outHeader.offset = alignedOffset;
            outHeader.size = size;

            return reinterpret_cast<void*>(ptr);
        }

        virtual void ResizeBuffer(size_t newMinSize) override
        {
            if (newMinSize <= bufferSize)
            {
                return;
            }

            newMinSize = MathUtil::NextPowerOf2(newMinSize);

            AllocatorType* allocator = GetDefaultAllocatorInstance<AllocatorType>();
            void* oldBuffer = raw;

            raw = allocator->Allocate(newMinSize, 16);
            HYP_CORE_ASSERT(raw != nullptr, "Failed to allocate memory for DeletionQueue");

            if (oldBuffer != nullptr)
            {
                Memory::Copy(raw, oldBuffer, bufferSize);
                allocator->Free(oldBuffer);
            }

            bufferSize = newMinSize;
        }

        void Free()
        {
            if (!raw)
            {
                return;
            }

            AllocatorType* allocator = GetDefaultAllocatorInstance<AllocatorType>();
            allocator->Free(raw);

            raw = nullptr;
            bufferSize = 0;
        }
    };

    static DeletionQueue& GetInstance();

    DeletionQueue();

    DeletionQueue(const DeletionQueue&) = delete;
    DeletionQueue& operator=(const DeletionQueue&) = delete;

    DeletionQueue(DeletionQueue&&) = delete;
    DeletionQueue& operator=(DeletionQueue&&) = delete;

    ~DeletionQueue();

    HYP_FORCE_INLINE bool IsInitialized() const
    {
        return m_isInitialized;
    }

    void Initialize();
    void Shutdown();

    void OnFrameEnd(uint32 prevFrameIndex);

    // returns number of entries that were deleted
    size_t ForceDeleteAll(uint32 bufferIndex);
    void Flush();

    // copy from temp entry list to sim thread / render thread queue
    void UpdateEntryListQueue();

    /*! \brief Allocate storage for a safe deleter of type T. The instance will need to be constructed using placement new by the caller.
        \param ppGuard Pointer-to-pointer of a mutex guard that will be set if locking is required. The caller is responsible for deleting the guard if set. */
    template <class T>
    DeletionQueueElem<T>* Alloc(Mutex::Guard** ppGuard)
    {
        static_assert(
            std::is_trivially_move_constructible_v<DeletionQueueElem<T>>
                && std::is_trivially_move_assignable_v<DeletionQueueElem<T>>
                && std::is_trivially_copyable_v<DeletionQueueElem<T>>,
            "DeletionQueueElem must be trivially moveable and trivially copyable");

        EntryHeader header {};

        EntryListBase& list = GetCurrentEntryList(ppGuard);

        DeletionQueueElem<T>* ptr = reinterpret_cast<DeletionQueueElem<T>*>(list.Alloc(sizeof(DeletionQueueElem<T>), alignof(DeletionQueueElem<T>), header));

        header.fc = GetCounterValue();
        header.destructFn = &DeletionQueueElem<T>::Destroy;

        list.Push(header);

        return ptr;
    }

    /*! \brief Allocate storage for custom deleter of type T. The instance will need to be constructed using placement new by the caller.
     *  Allows for an optional frame index in the range of 0..NumFramesInFlight for the deleter to be called on.
     *     If ~0u, will fall back to default behavior (when MinSafeDeleteCycles frames have passed)
     * \param destructFn Function pointer to the destructor function for T.
     * \param ppGuard Pointer-to-pointer of a mutex guard that will be set if locking is required. The caller is responsible for deleting the guard if set
     *  \param desiredIdx Desired frame index to delete on, or ~0u for next frame. */
    template <class T>
    T* AllocCustom(void (*destructFn)(void*), Mutex::Guard** ppGuard, uint32 desiredIdx = ~0u)
    {
        static_assert(is_pod_type_v<T>, "T must be a POD type");

        EntryHeader header {};

        EntryListBase& list = GetEntryList(ppGuard, desiredIdx);

        T* ptr = reinterpret_cast<T*>(list.Alloc(sizeof(T), alignof(T), header));

        header.fc = GetCounterValue();
        header.destructFn = destructFn;
        header.moveFn = nullptr;

        list.Push(header);

        return ptr;
    }

private:

    int64 GetCounterValue() const
    {
        return AtomicAdd(&m_counterValue, 0);
    }

    EntryListBase& GetCurrentEntryList(Mutex::Guard** ppGuard);
    EntryListBase& GetEntryList(Mutex::Guard** ppGuard, uint32 desiredIdx = ~0u);

    // for calling on another thread than sim thread / render thread.
    Mutex m_mutex;

    List<EntryList<DynamicAllocator>> m_tempEntryLists;
    volatile int32 m_tempEntryListCount = 0;

    FixedArray<EntryList<DynamicAllocator>*, RingBufferDepth> m_entryLists;

    // synchs to frame counter
    mutable volatile int64 m_counterValue;

    bool m_isInitialized;
};

template <class TFunction>
static inline void EnqueueDeletion(FunctionWrapper<TFunction>&& func)
{
    if (!func.func)
    {
        return;
    }

    DeletionQueue& instance = DeletionQueue::GetInstance();
    if (!instance.IsInitialized())
    {
        // just destruct it
        func();

        return;
    }

    Mutex::Guard* pGuard = nullptr;

    FunctionWrapper<TFunction>** ppPayload = instance.AllocCustom<FunctionWrapper<TFunction>*>(
        [](void* ptr)
        {
            FunctionWrapper<TFunction>* pPayload = *reinterpret_cast<FunctionWrapper<TFunction>**>(ptr);
            AssertDebug(pPayload != nullptr);

            (*pPayload)();

            delete pPayload;
        },
        &pGuard);

    *ppPayload = new FunctionWrapper<TFunction>(std::move(func));

    if (pGuard) // if locking was needed then we can delete the guard now to unlock.
    {
        delete pGuard;
    }
}

/*! \brief Defers deletion of a resource until enough frames have passed that the renderer can finish using it.
 *   It is garanteed that the number of frames before deletion is at least the number of frames before the sim thread and render thread will sync,
 *   so calling this function on the sim thread for example will ensure that the resource is not deleted until the render thread has a chance to finish using it. */
template <class T>
static inline void EnqueueDeletion(T&& value)
{
    DeletionQueue& instance = DeletionQueue::GetInstance();
    if (!instance.IsInitialized())
    {
        // just destruct it
        DeletionQueueElem<T>(std::forward<T>(value));
        return;
    }

    Mutex::Guard* pGuard = nullptr;
    DeletionQueueElem<T>* ptr = instance.Alloc<T>(&pGuard);
    new (ptr) DeletionQueueElem<T>(std::forward<T>(value));

    if (pGuard) // if locking was needed then we can delete the guard now to unlock.
    {
        delete pGuard;
    }
}

template <class T>
static inline void EnqueueDeletion(T* value)
{
    DeletionQueue& instance = DeletionQueue::GetInstance();
    if (!instance.IsInitialized())
    {
        // just destruct it
        DeletionQueueElem<std::remove_const_t<T>*> { value };
        return;
    }

    Mutex::Guard* pGuard = nullptr;
    DeletionQueueElem<std::remove_const_t<T>*>* ptr = instance.Alloc<std::remove_const_t<T>*>(&pGuard);
    new (ptr) DeletionQueueElem<std::remove_const_t<T>*>(value);

    if (pGuard) // if locking was needed then we can delete the guard now to unlock.
    {
        delete pGuard;
    }
}

/*! \see EnqueueDeletion(T&& value) */
template <class T, class AllocatorType>
static inline void EnqueueDeletion(Array<T, AllocatorType>&& value)
{
    if (value.Empty())
    {
        return;
    }

    for (auto& item : value)
    {
        EnqueueDeletion(std::move(item));
    }

    value.Clear();
}

/*! \see EnqueueDeletion(T&& value) */
template <class T, size_t Sz>
static inline void EnqueueDeletion(FixedArray<T, Sz>&& value)
{
    for (auto& it : value)
    {
        EnqueueDeletion(std::move(it));
    }

    value = {};
}

/*! \see EnqueueDeletion(T&& value) */
template <class T>
static inline void EnqueueDeletion(Set<T>&& value)
{
    for (auto& it : value)
    {
        EnqueueDeletion(std::move(it));
    }

    value.Clear();
}

} // namespace Hyperion
