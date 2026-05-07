/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/containers/LinkedList.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <rendering/RenderResult.hpp>

namespace Hyperion {

static constexpr uint32 MinSafeDeleteCycles = 10; // minimum number of cycles to wait before deleting an object

extern uint32 GetFrameCounter();

template <class T>
class DeletionQueueElem;

template <>
class DeletionQueueElem<Handle<ObjectBase>>
{
public:
    HYP_API explicit DeletionQueueElem(Handle<ObjectBase>&& handle)
        : DeletionQueueElem(handle.ptr)
    {
        handle.ptr = nullptr; // unset so DecRefStrong() doesn't get called on Handle destruction.
    }

    DeletionQueueElem(const DeletionQueueElem&) = delete;
    DeletionQueueElem& operator=(const DeletionQueueElem&) = delete;

    DeletionQueueElem(DeletionQueueElem&& other) noexcept
        : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    DeletionQueueElem& operator=(DeletionQueueElem&& other) noexcept
    {
        if (this != &other)
        {
            ptr = other.ptr;
            other.ptr = nullptr;
        }

        return *this;
    }

    HYP_API ~DeletionQueueElem();

protected:
    HYP_API explicit DeletionQueueElem(ObjectBase* ptr);

    ObjectBase* ptr;
};

template <class T>
class DeletionQueueElem<Handle<T>> final : public DeletionQueueElem<Handle<ObjectBase>>
{
public:
    HYP_API DeletionQueueElem(Handle<T>&& handle)
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

    DeletionQueueElem(const DeletionQueueElem&) = delete;
    DeletionQueueElem& operator=(const DeletionQueueElem&) = delete;

    DeletionQueueElem(DeletionQueueElem&& other) noexcept
        : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    DeletionQueueElem& operator=(DeletionQueueElem&& other) noexcept
    {
        if (this != &other)
        {
            ptr = other.ptr;
            other.ptr = nullptr;
        }

        return *this;
    }

    ~DeletionQueueElem()
    {
        delete ptr;
    }

protected:
    T* ptr;
};

/*! \brief Wrapper for a custom safe deleter type, with a ustom deleter function pointer. */
enum CustomDeleterTag
{
    CUSTOM_DELETER
};

template <class T>
class DeletionQueueElem final
{
    DeletionQueueElem() = delete;

public:
    DeletionQueueElem(const T& value, void (*deleteFn)(T&), CustomDeleterTag)
        : value(value),
          deleteFn(deleteFn)
    {
    }

    DeletionQueueElem(T&& value, void (*deleteFn)(T&), CustomDeleterTag)
        : value(std::move(value)),
          deleteFn(deleteFn)
    {
    }

    ~DeletionQueueElem()
    {
        if (deleteFn)
        {
            deleteFn(value);
        }
    }

private:
    T value;
    void (*deleteFn)(T&);
};

class HYP_API DeletionQueue
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

        virtual void* Alloc(uint32 size, uint32 alignment, EntryHeader& outHeader) = 0;
        virtual void ResizeBuffer(size_t newMinSize) = 0;

        void Push(const EntryHeader& header)
        {
            HYP_SCOPE;

            AssertDebug(currHeaders == &headers[0] || currHeaders == &headers[1]);

            currHeaders->PushBack(header);
        }

        // double-buffered to allow adding new entries while iterating.
        // we only actually iterate from headers[0] and move the entries that were added to headers[1] to headers[0] after iterating.
        Array<EntryHeader> headers[2];

        // current headers to iterate over (changed by calling SwapHeaderBuffers())
        Array<EntryHeader>* currHeaders;
        uint32 bufferPos;
        uint32 desiredIdx; // only for temp entry lists when we request a specific index!
    };

    template <class AllocatorType>
    class EntryList final : public EntryListBase
    {
    public:
        TByteBuffer<AllocatorType> buffer;

        EntryList()
            : EntryList(~0u)
        {
        }

        explicit EntryList(AllocatorType* pAllocator)
            : EntryListBase(~0u),
              buffer(pAllocator)
        {
        }

        explicit EntryList(uint32 desiredIdx)
            : EntryListBase(desiredIdx),
              buffer()
        {
        }

        EntryList(const EntryList&) = delete;
        EntryList& operator=(const EntryList&) = delete;

        EntryList(EntryList&&) noexcept = delete;
        EntryList& operator=(EntryList&&) noexcept = delete;

        ~EntryList() override = default;

        virtual void* Alloc(uint32 size, uint32 alignment, EntryHeader& outHeader) override
        {
            HYP_SCOPE;

            AssertDebug(alignment <= 16);

            const uint32 alignedOffset = ByteUtil::AlignAs(bufferPos, alignment);

            if (buffer.Size() < alignedOffset + size)
            {
                ResizeBuffer(alignedOffset + size);
            }

            void* ptr = buffer.Data() + alignedOffset;

            bufferPos = alignedOffset + size;

            outHeader = {};
            outHeader.offset = alignedOffset;
            outHeader.size = size;

            return ptr;
        }

        virtual void ResizeBuffer(size_t newMinSize) override
        {
            HYP_SCOPE;

            buffer.SetSize(newMinSize, /* zeroize */ false);
        }
    };

    static DeletionQueue& GetInstance();

    DeletionQueue();

    DeletionQueue(const DeletionQueue&) = delete;
    DeletionQueue& operator=(const DeletionQueue&) = delete;

    DeletionQueue(DeletionQueue&&) = delete;
    DeletionQueue& operator=(DeletionQueue&&) = delete;

    ~DeletionQueue();

    void Shutdown();

    /*! \brief Read the counter values for the last n frames, accumulated (n = num multi buffers).
     *   - only call this on the render thread.
     *  \param outNumElements The number of elements in the deletion queue.
     *  \param outTotalBytes The total size of the deletion queue in bytes. */
    void GetCounterValues(uint32& outNumElements, uint32& outTotalBytes) const;

    int Iterate(int maxIter = 1000);

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
        EntryHeader header;

        EntryListBase& list = GetCurrentEntryList(ppGuard);

        DeletionQueueElem<T>* ptr = reinterpret_cast<DeletionQueueElem<T>*>(list.Alloc(sizeof(DeletionQueueElem<T>), alignof(DeletionQueueElem<T>), header));

        header.fc = GetFrameCounter();

        if constexpr (!std::is_trivially_destructible_v<DeletionQueueElem<T>>)
        {
            header.destructFn = [](void* ptr)
            {
                DeletionQueueElem<T>* ptrCasted = reinterpret_cast<DeletionQueueElem<T>*>(ptr);
                ptrCasted->~DeletionQueueElem<T>();
            };
        }
        else
        {
            header.destructFn = nullptr;
        }

        if constexpr (!std::is_trivially_move_assignable_v<DeletionQueueElem<T>>)
        {
            header.moveFn = [](void* dst, void* src)
            {
                new (dst) DeletionQueueElem<T>(std::move(*reinterpret_cast<DeletionQueueElem<T>*>(src)));

                // destruct prev in place, since we won't be using it again (this is done upon buffer resize)
                reinterpret_cast<DeletionQueueElem<T>*>(src)->~DeletionQueueElem();
            };
        }
        else
        {
            header.moveFn = nullptr;
        }

        list.Push(header);

        return ptr;
    }

    /*! \brief Allocate storage for custom deleter of type T. The instance will need to be constructed using placement new by the caller.
     *  Allows for an optional frame index for the deleter to be called on. If ~0u, will be called on the next frame.
     * \param destructFn Function pointer to the destructor function for T.
     * \param ppGuard Pointer-to-pointer of a mutex guard that will be set if locking is required. The caller is responsible for deleting the guard if set
     *  \param desiredIdx Desired frame index to delete on, or ~0u for next frame. */
    template <class T>
    T* AllocCustom(void (*destructFn)(void*), Mutex::Guard** ppGuard, uint32 desiredIdx = ~0u)
    {
        static_assert(is_pod_type_v<T>, "T must be a POD type");

        EntryHeader header;

        EntryListBase& list = GetEntryList(ppGuard, desiredIdx);

        T* ptr = reinterpret_cast<T*>(list.Alloc(sizeof(T), alignof(T), header));

        header.fc = GetFrameCounter();
        header.destructFn = destructFn;
        header.moveFn = nullptr;

        list.Push(header);

        return ptr;
    }

private:
    struct Counter
    {
        uint32 numElements = 0;
        uint32 numTotalBytes = 0;
    };

    EntryListBase& GetCurrentEntryList(Mutex::Guard** ppGuard);
    EntryListBase& GetEntryList(Mutex::Guard** ppGuard, uint32 desiredIdx = ~0u);
    void UpdateCounter(uint32 bufferIndex);

    // for calling on another thread than sim thread / render thread.
    Mutex m_mutex;

    LinkedList<EntryList<DynamicAllocator>> m_tempEntryLists;
    volatile int32 m_tempEntryListCount = 0;

    FixedArray<EntryList<DynamicAllocator>*, RingBufferDepth> m_entryLists;
    Counter m_counters[RingBufferDepth];
};

template <class TFunction>
static inline void EnqueueDeletion(FunctionWrapper<TFunction>&& func)
{
    Mutex::Guard* pGuard = nullptr;

    FunctionWrapper<TFunction>** ppPayload = DeletionQueue::GetInstance().AllocCustom<FunctionWrapper<TFunction>*>([](void* ptr)
        {
            AssertOnThread(g_renderThread);

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
    Mutex::Guard* pGuard = nullptr;
    DeletionQueueElem<T>* ptr = DeletionQueue::GetInstance().Alloc<T>(&pGuard);
    new (ptr) DeletionQueueElem<T>(std::forward<T>(value));

    if (pGuard) // if locking was needed then we can delete the guard now to unlock.
    {
        delete pGuard;
    }
}

template <class T>
static inline void EnqueueDeletion(T* value)
{
    Mutex::Guard* pGuard = nullptr;
    DeletionQueueElem<std::remove_const_t<T>*>* ptr = DeletionQueue::GetInstance().Alloc<std::remove_const_t<T>*>(&pGuard);
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
        if (!it.IsValid())
        {
            continue;
        }

        EnqueueDeletion(std::move(it));
    }

    value = {};
}

/*! \see EnqueueDeletion(T&& value) */
template <class T>
static inline void EnqueueDeletion(HashSet<T>&& value)
{
    for (auto& it : value)
    {
        if (!it.IsValid())
        {
            continue;
        }

        EnqueueDeletion(std::move(it));
    }

    value.Clear();
}

} // namespace Hyperion
