/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderInterface.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <scripting/ScriptObjectResource.hpp>

namespace Hyperion {

HYP_API SafeDeleter* GetSafeDeleterInstance()
{
    AssertDebug(g_safeDeleter != nullptr);
    return g_safeDeleter;
}

#pragma region SafeDeleterEntry<Handle<ObjectBase>>

SafeDeleterEntry<Handle<ObjectBase>>::SafeDeleterEntry(ObjectBase* ptr)
    : ptr(ptr)
{
    if (ptr)
    {
#ifdef HYP_DOTNET
        ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
        const bool hasExtraRef = scriptObjectResource && (scriptObjectResource->GetScriptLanguageMask() == (1u << uint32(ScriptLanguage::CSharp)));
#else
        const bool hasExtraRef = false;
#endif
        ObjectHeader* header = ptr->GetObjectHeader_Internal();

        int32 currentCount = AtomicIncrement(&header->refCountStrong);
        AssertDebug(currentCount > 1); // should have another ref from input

        while (currentCount != 1 + (hasExtraRef ? 1 : 0))
        {
            if (AtomicCompareExchange(&header->refCountStrong, currentCount, currentCount - 1))
            {

#ifdef HYP_DOTNET
                if (hasExtraRef)
                {
                    Object_DecScriptObjectRef(ptr);
                }
#endif

                break;
            }
        }
    }
}

SafeDeleterEntry<Handle<ObjectBase>>::~SafeDeleterEntry()
{
    // call destructor if no more strong references
    if (ptr)
    {
        ObjectHeader* header = ptr->GetObjectHeader_Internal();

        /// @NOTE: Objects with C# scripts that haven't been GC'd here wouldn't get deleted.
        /// However, we incremented the strong ref count in the constructor to prevent deletion until this point.
        /// So the object would've been kept alive long enough to be safe to use during rendering.
        /// When the .NET GC runs, it will decrement the strong ref count and delete the object immediately if it reaches 0.
        if (AtomicDecrement(&header->refCountStrong) == 0)
        {
            // we increment weak reference to prevent weak refs to this from causing Release() upon calling their destructors.
            header->IncRefWeak();

            ptr->~ObjectBase();

            // this will free the slot if no other weak references remain
            header->DecRefWeak();
        }
    }
}

#pragma region SafeDeleterEntry<Handle<ObjectBase>>

#pragma region SafeDeleter

SafeDeleter::SafeDeleter()
    : m_entryLists { nullptr },
      m_tempEntryListCount(0)
{
    for (uint32 i = 0; i < RingBufferDepth; i++)
    {
        m_entryLists[i] = new SafeDeleter::EntryList<DynamicAllocator>();
    }
}

SafeDeleter::~SafeDeleter()
{
    HYP_NAMED_SCOPE("SafeDeleter::~SafeDeleter");

    AssertOnThread(g_renderThread);

    auto deleteAll = [](auto& entryList)
    {
        // free all entries in the list
        for (uint32 i = 0; i < 2; ++i)
        {
            for (EntryHeader& header : entryList.headers[i])
            {
                if (header.destructFn)
                {
                    header.destructFn(reinterpret_cast<void*>(entryList.buffer.Data() + header.offset));
                }
            }

            entryList.headers[i].Clear();
        }

        entryList.currHeaders = &entryList.headers[0];
        entryList.buffer.Clear();
        entryList.bufferPos = 0;
    };

    // delete all entries in all buffers
    for (auto* pEntryList : m_entryLists)
    {
        deleteAll(*pEntryList);

        delete pEntryList;
    }

    // free all temp entry lists
    for (auto& it : m_tempEntryLists)
    {
        deleteAll(it);
    }
}

void SafeDeleter::GetCounterValues(uint32& outNumElements, uint32& outTotalBytes) const
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    outNumElements = 0;
    outTotalBytes = 0;

    for (const Counter& counter : m_counters)
    {
        outNumElements += counter.numElements;
        outTotalBytes += counter.numTotalBytes;
    }
}

int SafeDeleter::Iterate(int maxIter)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    uint32 bufferIndex = RenderApi::GetRingIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& entryList = *m_entryLists[bufferIndex];

    Array<EntryHeader>& headers = *entryList.currHeaders;
    entryList.SwapHeaderBuffers();

    const uint32 frameCounter = RenderApi::GetFrameCounter();

    int iterCount = 0;
    for (auto it = headers.Begin(); iterCount < maxIter && it != headers.End(); ++iterCount)
    {
        EntryHeader header = *it;

        if ((int64(frameCounter) - int64(header.fc)) < MinSafeDeleteCycles)
        {
            ++it;
            continue; // skip this entry, it will be processed again next frame
        }

        if (header.destructFn)
        {
            AssertDebug(header.offset < entryList.buffer.Size());
            AssertDebug(header.size <= entryList.buffer.Size() - header.offset);

            header.destructFn(reinterpret_cast<void*>(entryList.buffer.Data() + header.offset));
        }

        it = headers.Erase(it); // remove the entry from the list
    }

    // concat any headers that were added while iterating to our list
    headers.Concat(*entryList.currHeaders);
    entryList.currHeaders->Clear();

    // swap the buffers back to original state now that we are done iterating
    entryList.currHeaders = &headers;

    if (headers.Empty())
    {
        // clear buffer if all entries have been deleted
        entryList.buffer.Clear();
        entryList.bufferPos = 0;
    }
    else
    {
        const uint32 firstOffset = headers[0].offset; // so we can subtract it from all offsets after resizing

        for (SizeType headerIdx = 0; headerIdx < headers.Size(); headerIdx++)
        {
            EntryHeader& header = headers[headerIdx];

            AssertDebug(header.offset >= firstOffset);
            AssertDebug(entryList.buffer.Size() >= header.offset + header.size);
            header.offset -= firstOffset;
        }

        const uint32 newSize = headers.Back().offset + headers.Back().size;

        // Move elements to the front of the buffer
        Memory::Move(
            entryList.buffer.Data(),
            entryList.buffer.Data() + firstOffset,
            newSize);

        // compact the buffer to the new size, if size is 20% larger than it needs to be
        if (entryList.buffer.Size() > newSize * 1.2)
        {
            entryList.buffer.SetSize(newSize);
        }

        entryList.bufferPos = newSize;
    }

    return iterCount;
}

int SafeDeleter::ForceDeleteAll(uint32 bufferIndex)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& entryList = *m_entryLists[bufferIndex];

    int iterCount = 0;

    AssertDebug(entryList.currHeaders == &entryList.headers[0]);
    AssertDebug(entryList.headers[1].Empty());

    while (!entryList.headers[0].Empty())
    {
        EntryHeader header = entryList.headers[0].Front();
        entryList.headers[0].PopFront();

        if (header.destructFn)
        {
            header.destructFn(reinterpret_cast<void*>(entryList.buffer.Data() + header.offset));
        }

        ++iterCount;
    }

    // clear buffer if all entries have been deleted
    entryList.buffer.Clear();
    entryList.bufferPos = 0;

    return iterCount;
}

void SafeDeleter::UpdateCounter(uint32 bufferIndex)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& entryList = *m_entryLists[bufferIndex];
    Counter& counter = m_counters[bufferIndex];

    counter.numElements = entryList.currHeaders->Size();
    counter.numTotalBytes = entryList.buffer.Size();
}

void SafeDeleter::UpdateEntryListQueue()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    uint32 bufferIndex = RenderApi::GetRingIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& currentEntryList = *m_entryLists[bufferIndex];

    int32 numTempEntryLists = 0;
    if (AtomicAdd(&m_tempEntryListCount, 0) == 0)
    {
        // no temp entry lists, nothing to append to our list,
        // so just update counter and return
        UpdateCounter(bufferIndex);
        return;
    }

    Mutex::Guard guard(m_mutex);
    for (auto it = m_tempEntryLists.Begin(); it != m_tempEntryLists.End();)
    {
        auto& entryList = *it;

        if (entryList.desiredIdx != ~0u && entryList.desiredIdx != bufferIndex)
        {
            // not desired index, skip for now (will be picked up by next iter that matches)
            ++it;
            continue;
        }

        if (entryList.bufferPos == 0)
        {
            // no data in buffers, skip
            it = m_tempEntryLists.Erase(it);
            AtomicDecrement(&m_tempEntryListCount);

            continue;
        }

        AssertDebug(entryList.currHeaders == &entryList.headers[0]);
        AssertDebug(entryList.headers[1].Empty());

        Array<EntryHeader>& itHeaders = *entryList.currHeaders;
        entryList.SwapHeaderBuffers();

        // concat all lists and take ownership of the data
        for (EntryHeader& header : itHeaders)
        {
            const uint32 newAlignedOffset = ByteUtil::AlignAs(currentEntryList.bufferPos, 16);

            void* vp = entryList.buffer.Data() + header.offset;

            if (currentEntryList.buffer.Size() < newAlignedOffset + header.size)
            {
                currentEntryList.ResizeBuffer(newAlignedOffset + header.size);
            }

            if (header.moveFn)
            {
                header.moveFn(reinterpret_cast<void*>(currentEntryList.buffer.Data() + newAlignedOffset), vp);
            }
            else
            {
                Memory::Copy(currentEntryList.buffer.Data() + newAlignedOffset, vp, header.size);
            }

            if (header.destructFn)
            {
                header.destructFn(vp);
            }

            header.offset = newAlignedOffset;

            currentEntryList.bufferPos = newAlignedOffset + header.size;

            currentEntryList.currHeaders->PushBack(header);
        }

        entryList.currHeaders->Clear();
        entryList.currHeaders = &entryList.headers[0];

        entryList.buffer.Clear();
        entryList.bufferPos = 0;

        it = m_tempEntryLists.Erase(it);

        AtomicDecrement(&m_tempEntryListCount);
    }

    UpdateCounter(bufferIndex);
}

#pragma endregion SafeDeleter

#pragma region SafeDeleter::EntryList

SafeDeleter::EntryListBase& SafeDeleter::GetCurrentEntryList(Mutex::Guard** ppGuard)
{
    HYP_SCOPE;

    AssertDebug(ppGuard != nullptr);
    *ppGuard = nullptr;

    if (IsOnThread(g_simThread | g_renderThread))
    {
        uint32 bufferIndex = RenderApi::GetRingIndex();
        AssertDebug(bufferIndex < m_entryLists.Size());

        return *m_entryLists[bufferIndex];
    }

    *ppGuard = new Mutex::Guard(m_mutex);

    AtomicIncrement(&m_tempEntryListCount);
    auto& entryList = m_tempEntryLists.EmplaceBack();

    return entryList;
}

SafeDeleter::EntryListBase& SafeDeleter::GetEntryList(Mutex::Guard** ppGuard, uint32 desiredIdx)
{
    // If:
    //  - desiredIdx == ~0u (not specified) OR
    //  - On render thread and desiredIdx == CURRENT idx
    // we use the CURRENT entry list
    if (desiredIdx == ~0u || (IsOnThread(g_renderThread) && desiredIdx == RenderApi::GetRingIndex()))
    {
        return GetCurrentEntryList(ppGuard);
    }

    // if desiredIdx is specified (wants to be deleted on a specific frame),
    // we need to create a new entry list.

    *ppGuard = new Mutex::Guard(m_mutex);

    AtomicIncrement(&m_tempEntryListCount);
    auto& entryList = m_tempEntryLists.EmplaceBack(desiredIdx);

    return entryList;
}

#pragma endregion SafeDeleter::EntryList

} // namespace Hyperion
