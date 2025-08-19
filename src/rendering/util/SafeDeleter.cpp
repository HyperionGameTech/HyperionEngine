/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderGlobalState.hpp>

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

SafeDeleter::EntryList::EntryList()
    : buffer(),
      currHeaders(&headers[0]),
      bufferPos(0)
{
}

SafeDeleter::SafeDeleter()
    : m_entryLists {},
      m_tempEntryListCount(0)
{
}

SafeDeleter::~SafeDeleter()
{
    HYP_NAMED_SCOPE("SafeDeleter::~SafeDeleter");

    auto deleteAll = [](EntryList& entryList)
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
        entryList.buffer = ByteBuffer();
        entryList.bufferPos = 0;
    };

    // delete all entries in all buffers
    for (EntryList& it : m_entryLists)
    {
        deleteAll(it);
    }

    // free all temp entry lists
    for (EntryList& it : m_tempEntryLists)
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

    uint32 bufferIndex = RenderApi_GetFrameIndex();
    AssertDebug(bufferIndex < HYP_ARRAY_SIZE(m_entryLists));

    SafeDeleter::EntryList& entryList = m_entryLists[bufferIndex];

    Array<EntryHeader>& headers = *entryList.currHeaders;
    entryList.SwapHeaderBuffers();

    const uint32 frameCounter = RenderApi_GetFrameCounter();

    int iterCount = 0;
    for (auto it = headers.Begin(); iterCount < maxIter && it != headers.End(); ++iterCount)
    {
        EntryHeader header = *it;

        if ((frameCounter - header.fc) < g_minSafeDeleteCycles)
        {
            ++it;
            continue; // skip this entry, it will be processed again next frame
        }

        if (header.destructFn)
        {
            HYP_LOG_TEMP("Deleting entry with offset {}", header.offset);
            AssertDebug(header.offset < entryList.buffer.Size());
            AssertDebug(header.size <= entryList.buffer.Size() - header.offset);
            header.destructFn(reinterpret_cast<void*>(entryList.buffer.Data() + header.offset));
        }

        it = headers.Erase(it); // remove the entry from the list
    }

    // concat any headers that were added while iterating to our list
    headers.Concat(std::move(*entryList.currHeaders));

    // swap the buffers back to original state now that we are done iterating
    entryList.currHeaders = &headers;

    if (headers.Empty())
    {
        // clear buffer if all entries have been deleted
        entryList.buffer = ByteBuffer();
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

    AssertDebug(bufferIndex < HYP_ARRAY_SIZE(m_entryLists));

    SafeDeleter::EntryList& entryList = m_entryLists[bufferIndex];

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
    entryList.buffer = ByteBuffer();
    entryList.bufferPos = 0;

    return iterCount;
}

SafeDeleter::EntryList& SafeDeleter::GetCurrentEntryList()
{
    HYP_SCOPE;

    if (Threads::IsOnThread(g_gameThread | g_renderThread))
    {
        uint32 bufferIndex = RenderApi_GetFrameIndex();
        AssertDebug(bufferIndex < HYP_ARRAY_SIZE(m_entryLists));

        return m_entryLists[bufferIndex];
    }

    Mutex::Guard guard(m_mutex);

    AtomicIncrement(&m_tempEntryListCount);
    SafeDeleter::EntryList& entryList = m_tempEntryLists.EmplaceBack();

    return entryList;
}

void SafeDeleter::UpdateCounter(uint32 bufferIndex)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(bufferIndex < HYP_ARRAY_SIZE(m_entryLists));

    SafeDeleter::EntryList& entryList = m_entryLists[bufferIndex];
    SafeDeleter::Counter& counter = m_counters[bufferIndex];

    counter.numElements = entryList.currHeaders->Size();
    counter.numTotalBytes = entryList.buffer.Size();
}

void SafeDeleter::UpdateEntryListQueue()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    uint32 bufferIndex = RenderApi_GetFrameIndex();
    AssertDebug(bufferIndex < HYP_ARRAY_SIZE(m_entryLists));

    SafeDeleter::EntryList& currentEntryList = m_entryLists[bufferIndex];

    int32 numTempEntryLists = 0;
    if (AtomicAdd(&m_tempEntryListCount, 0) == 0)
    {
        // no temp entry lists, nothing to append to our list,
        // so just update counter and return
        UpdateCounter(bufferIndex);
        return;
    }

    Mutex::Guard guard(m_mutex);
    for (EntryList& it : m_tempEntryLists)
    {
        if (it.bufferPos == 0)
        {
            // no data in buffers, skip
            continue;
        }

        AssertDebug(it.currHeaders == &it.headers[0]);
        AssertDebug(it.headers[1].Empty());

        Array<EntryHeader>& itHeaders = *it.currHeaders;
        it.SwapHeaderBuffers();

        // concat all lists and take ownership of the data
        for (EntryHeader& header : itHeaders)
        {
            const uint32 newAlignedOffset = ByteUtil::AlignAs(currentEntryList.bufferPos, 16);

            void* vp = it.buffer.Data() + header.offset;

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

        it.currHeaders->Clear();
        it.currHeaders = &it.headers[0];

        it.buffer = ByteBuffer();
        it.bufferPos = 0;
    }

    m_tempEntryLists.Clear();

    AtomicExchange(&m_tempEntryListCount, 0);

    UpdateCounter(bufferIndex);
}

void* SafeDeleter::EntryList::Alloc(uint32 size, uint32 alignment, EntryHeader& outHeader)
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

void SafeDeleter::EntryList::Push(const EntryHeader& header)
{
    HYP_SCOPE;
    currHeaders->PushBack(header);
}

void SafeDeleter::EntryList::ResizeBuffer(SizeType newMinSize)
{
    HYP_SCOPE;

    buffer.SetSize(newMinSize, /* zeroize */ false);
}

} // namespace hyperion
