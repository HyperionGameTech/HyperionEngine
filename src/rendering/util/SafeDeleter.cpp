/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderGlobalState.hpp>

// temp
#include <rendering/Texture.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

HYP_API SafeDeleter* GetSafeDeleterInstance()
{
    AssertDebug(g_safeDeleter != nullptr);
    return g_safeDeleter;
}

#pragma region SafeDeleterEntry < HypObjectBase*>

SafeDeleterEntry<HypObjectBase*>::SafeDeleterEntry(HypObjectBase* ptr, ConstructFromHandleTag)
    : ptr(ptr)
{
    if (ptr)
    {
        const bool hasScriptObjectResource = ptr->GetScriptObjectResource() != nullptr;

        // ADD weak ref, so no other releases of weak references when our strong count is zero triggers a Release()
        ptr->GetObjectHeader_Internal()->IncRefWeak();

        int32 count = AtomicAdd(&ptr->GetObjectHeader_Internal()->refCountStrong, 0);

        // manually decrease strong refcount by 1
        // ternary is here due to the fact that the c# object would have an extra ref.
        while (count != (hasScriptObjectResource ? 1 : 0))
        {
            if (AtomicCompareExchange(&ptr->GetObjectHeader_Internal()->refCountStrong, count, count - 1))
            {

#ifdef HYP_DOTNET
                if (hasScriptObjectResource)
                    HypObject_DecScriptObjectRef(ptr);
#endif

                break;
            }
        }
    }
}

SafeDeleterEntry<HypObjectBase*>::~SafeDeleterEntry()
{
    // call destructor if no more strong references
    if (ptr)
    {
        HypObjectHeader* header = ptr->GetObjectHeader_Internal();
        AssertDebug(header != nullptr); // weird bug?

        if (AtomicAdd(&header->refCountStrong, 0) == 0)
        {
            ptr->~HypObjectBase();
            
            // this will free the slot if no other weak references remain
            header->DecRefWeak();
        }
    }
}

#pragma region SafeDeleterEntry < HypObjectBase*>

#pragma region SafeDeleter

SafeDeleter::SafeDeleter()
    : m_entryLists { nullptr },
      m_tempEntryListCount(0)
{
    for (uint32 i = 0; i < NumMultiBuffers; i++)
    {
        m_entryLists[i] = new SafeDeleter::EntryList<Pool>(g_framePools[i]);
    }
}

SafeDeleter::~SafeDeleter()
{
    HYP_NAMED_SCOPE("SafeDeleter::~SafeDeleter");

    Threads::AssertOnThread(g_renderThread);

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
    for (EntryList<Pool>* it : m_entryLists)
    {
        deleteAll(*it);

        delete it;
    }

    // free all temp entry lists
    for (EntryList<DynamicAllocator>& it : m_tempEntryLists)
    {
        deleteAll(it);
    }
}

void SafeDeleter::GetCounterValues(uint32& outNumElements, uint32& outTotalBytes) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

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
    Threads::AssertOnThread(g_renderThread);

    uint32 bufferIndex = RenderApi::GetFrameIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    SafeDeleter::EntryList<Pool>& entryList = *m_entryLists[bufferIndex];

    Array<EntryHeader>& headers = *entryList.currHeaders;
    entryList.SwapHeaderBuffers();

    const uint32 frameCounter = RenderApi::GetFrameCounter();

    int iterCount = 0;
    for (auto it = headers.Begin(); iterCount < maxIter && it != headers.End(); ++iterCount)
    {
        EntryHeader header = *it;

        if ((frameCounter - header.fc) < MinSafeDeleteCycles)
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
        Memory::MemMove(
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
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(bufferIndex < m_entryLists.Size());

    SafeDeleter::EntryList<Pool>& entryList = *m_entryLists[bufferIndex];

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
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(bufferIndex < m_entryLists.Size());

    SafeDeleter::EntryList<Pool>& entryList = *m_entryLists[bufferIndex];
    SafeDeleter::Counter& counter = m_counters[bufferIndex];

    counter.numElements = entryList.currHeaders->Size();
    counter.numTotalBytes = entryList.buffer.Size();
}

void SafeDeleter::UpdateEntryListQueue()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    uint32 bufferIndex = RenderApi::GetFrameIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    SafeDeleter::EntryList<Pool>& currentEntryList = *m_entryLists[bufferIndex];

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
        EntryList<DynamicAllocator>& entryList = *it;

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
                Memory::MemCpy(currentEntryList.buffer.Data() + newAlignedOffset, vp, header.size);
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

    if (Threads::IsOnThread(g_gameThread | g_renderThread))
    {
        uint32 bufferIndex = RenderApi::GetFrameIndex();
        AssertDebug(bufferIndex < m_entryLists.Size());

        return *m_entryLists[bufferIndex];
    }

    *ppGuard = new Mutex::Guard(m_mutex);

    AtomicIncrement(&m_tempEntryListCount);
    SafeDeleter::EntryList<DynamicAllocator>& entryList = m_tempEntryLists.EmplaceBack();

    return entryList;
}

SafeDeleter::EntryListBase& SafeDeleter::GetEntryList(Mutex::Guard** ppGuard, uint32 desiredIdx)
{
    // If:
    //  - desiredIdx == ~0u (not specified) OR
    //  - On render thread and desiredIdx == CURRENT idx
    // we use the CURRENT entry list
    if (desiredIdx == ~0u || (Threads::IsOnThread(g_renderThread) && desiredIdx == RenderApi::GetFrameIndex()))
    {
        return GetCurrentEntryList(ppGuard);
    }

    // if desiredIdx is specified (wants to be deleted on a specific frame),
    // we need to create a new entry list.

    *ppGuard = new Mutex::Guard(m_mutex);

    AtomicIncrement(&m_tempEntryListCount);
    SafeDeleter::EntryList<DynamicAllocator>& entryList = m_tempEntryLists.EmplaceBack(desiredIdx);

    return entryList;
}

#pragma endregion SafeDeleter::EntryList

} // namespace hyperion
