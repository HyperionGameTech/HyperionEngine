/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/Constants.hpp>
#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/threading/Mutex.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderResult.hpp>

namespace hyperion {

static constexpr uint32 g_minSafeDeleteCycles = 10; // minimum number of cycles to wait before deleting an object

HYP_API extern uint32 RenderApi_GetFrameCounter();

class SafeDeleterEntryBase
{
protected:
    friend class SafeDeleter;

    SafeDeleterEntryBase() = default;
    ~SafeDeleterEntryBase() = default;
};

template <class T>
class SafeDeleterEntry;

template <class T>
class SafeDeleterEntry<Handle<T>> final : public SafeDeleterEntryBase
{
public:
    SafeDeleterEntry(Handle<T>&& handle)
        : handle(std::move(handle))
    {
    }

protected:
    Handle<T> handle;
};

template <>
class SafeDeleterEntry<AnyHandle> final : public SafeDeleterEntryBase
{
public:
    SafeDeleterEntry(AnyHandle&& handle)
        : handle(std::move(handle))
    {
    }

protected:
    AnyHandle handle;
};

class HYP_API SafeDeleter
{
    static constexpr uint32 g_numMultiBuffers = g_tripleBuffer ? 3 : 2;

public:
    struct EntryHeader
    {
        uint32 offset = 0;
        uint32 size = 0;
        uint32 fc = 0; // frame counter when the entry was added

        void (*moveFn)(void*, void*) = nullptr;
        void (*destructFn)(void*) = nullptr;
    };

    struct EntryList
    {
        ByteBuffer buffer;
        // double-buffered to allow adding new entries while iterating.
        // we only actually iterate from headers[0] and move the entries that were added to headers[1] to headers[0] after iterating.
        Array<EntryHeader> headers[2];

        // current headers to iterate over (changed by calling SwapHeaderBuffers())
        Array<EntryHeader>* currHeaders;
        uint32 bufferOffset;

        EntryList();

        void SwapHeaderBuffers()
        {
            currHeaders = (currHeaders == &headers[0]) ? &headers[1] : &headers[0];
        }

        void* Alloc(uint32 size, uint32 alignment, EntryHeader& outHeader);
        void Push(const EntryHeader& header);
        void ResizeBuffer(SizeType newMinSize);
    };

    SafeDeleter();
    SafeDeleter(const SafeDeleter&) = delete;
    SafeDeleter& operator=(const SafeDeleter&) = delete;
    SafeDeleter(SafeDeleter&&) = delete;
    SafeDeleter& operator=(SafeDeleter&&) = delete;
    ~SafeDeleter();

    int Iterate(int maxIter = 1000);

    // returns number of entries that were deleted
    int ForceDeleteAll(uint32 bufferIndex);

    // copy from temp entry list to game thread / render thread queue
    void UpdateEntryListQueue();

    template <class T>
    SafeDeleterEntry<T>* Alloc()
    {
        EntryHeader header;

        EntryList& list = GetCurrentEntryList();

        SafeDeleterEntry<T>* ptr = reinterpret_cast<SafeDeleterEntry<T>*>(list.Alloc(sizeof(SafeDeleterEntry<T>), alignof(SafeDeleterEntry<T>), header));

        header.fc = RenderApi_GetFrameCounter();

        if constexpr (!std::is_trivially_destructible_v<SafeDeleterEntry<T>>)
        {
            header.destructFn = [](void* ptr)
            {
                T* ptrCasted = reinterpret_cast<T*>(ptr);
                ptrCasted->~T();
            };
            
            //header.destructFn = &Memory::Destruct<SafeDeleterEntry<T>>;
        }
        else
        {
            header.destructFn = nullptr;
        }

        if constexpr (!std::is_trivially_move_assignable_v<SafeDeleterEntry<T>>)
        {
            header.moveFn = [](void* dst, void* src)
            {
                new (dst) SafeDeleterEntry<T>(std::move(*reinterpret_cast<SafeDeleterEntry<T>*>(src)));
            };
        }
        else
        {
            header.moveFn = nullptr;
        }

        list.Push(header);

        return ptr;
    }

private:
    EntryList& GetCurrentEntryList();

    // for calling on another thread than game thread / render thread.
    Mutex m_mutex;

    LinkedList<EntryList> m_tempEntryLists;
    volatile int32 m_tempEntryListCount = 0;

    FixedArray<EntryList, g_numMultiBuffers> m_entryLists;
};

extern HYP_API SafeDeleter* GetSafeDeleterInstance();

template <class T>
static inline void SafeDelete(T&& value)
{
    SafeDeleterEntry<T>* ptr = GetSafeDeleterInstance()->Alloc<T>();
    new (ptr) SafeDeleterEntry<T>(std::forward<T>(value));
}

template <class T, class AllocatorType>
static inline void SafeDelete(Array<T, AllocatorType>&& value)
{
    for (auto& item : value)
    {
        SafeDelete(std::move(item));
    }

    value.Clear();
}

template <class T, SizeType Sz>
static inline void SafeDelete(FixedArray<T, Sz>&& value)
{
    for (auto& it : value)
    {
        if (!it.IsValid())
        {
            continue;
        }

        SafeDelete(std::move(it));
    }

    value = {};
}

template <class T, auto KeyBy>
static inline void SafeDelete(HashSet<T, KeyBy>&& value)
{
    for (auto& it : value)
    {
        if (!it.IsValid())
        {
            continue;
        }

        SafeDelete(std::move(it));
    }

    value.Clear();
}

} // namespace hyperion
