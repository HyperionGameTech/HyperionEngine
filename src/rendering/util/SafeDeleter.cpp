/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderGlobalState.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>

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
      bufferOffset(0)
{
}

// static inline void AddToEntryList(SafeDeleter::EntryList& entryList, const SafeDeleter::Entry& entry)
// {
//     auto reverseMapIt = entryList.reverseMap.Find(entry.ptr);
//     AssertDebug(reverseMapIt == entryList.reverseMap.End(), "ptr {} added multiple times to deletion queue", entry.ptr);

//     const int32 idx = AtomicIncrement(&entryList.count) - 1;
//     AssertDebug(idx >= 0);

//     SafeDeleter::Entry* pEntry = &*entryList.entries.Emplace(idx, entry);
//     entryList.reverseMap.Insert(entry.ptr, pEntry);
// }

SafeDeleter::SafeDeleter()
    : m_entryLists {},
      m_tempEntryListCount(0)
{
}

SafeDeleter::~SafeDeleter()
{
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
        entryList.bufferOffset = 0;
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

int SafeDeleter::Iterate(int maxIter)
{
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    uint32 bufferIndex = RenderApi_GetFrameIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

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
        entryList.bufferOffset = 0;
    }

    return iterCount;
}

int SafeDeleter::ForceDeleteAll(uint32 bufferIndex)
{
    AssertDebug(bufferIndex < m_entryLists.Size());

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
    entryList.bufferOffset = 0;

    return iterCount;
}

SafeDeleter::EntryList& SafeDeleter::GetCurrentEntryList()
{
    if (Threads::IsOnThread(g_gameThread | g_renderThread))
    {
        uint32 bufferIndex = RenderApi_GetFrameIndex();
        AssertDebug(bufferIndex < m_entryLists.Size());

        return m_entryLists[bufferIndex];
    }

    Mutex::Guard guard(m_mutex);

    AtomicIncrement(&m_tempEntryListCount);
    SafeDeleter::EntryList& entryList = m_tempEntryLists.EmplaceBack();

    return entryList;
}

void SafeDeleter::UpdateEntryListQueue()
{
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    // move all entries over to current (needs to update pointers as well)

    uint32 bufferIndex = RenderApi_GetFrameIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    SafeDeleter::EntryList& currentEntryList = m_entryLists[bufferIndex];

    int32 numTempEntryLists = 0;
    if (AtomicAdd(&m_tempEntryListCount, 0) == 0)
    {
        // no temp entry lists, nothing to do
        return;
    }

    Mutex::Guard guard(m_mutex);
    for (EntryList& it : m_tempEntryLists)
    {
        if (it.bufferOffset == 0)
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
            const uint32 newAlignedOffset = ByteUtil::AlignAs(currentEntryList.bufferOffset, 16);

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

            currentEntryList.bufferOffset = newAlignedOffset + header.size;

            currentEntryList.currHeaders->PushBack(header);
        }

        it.currHeaders->Clear();
        it.currHeaders = &it.headers[0];

        it.buffer = ByteBuffer();
        it.bufferOffset = 0;
    }

    m_tempEntryLists.Clear();

    AtomicExchange(&m_tempEntryListCount, 0);
}

void* SafeDeleter::EntryList::Alloc(uint32 size, uint32 alignment, EntryHeader& outHeader)
{
    HYP_SCOPE;

    AssertDebug(alignment <= 16);

    const uint32 alignedOffset = ByteUtil::AlignAs(bufferOffset, alignment);

    if (buffer.Size() < alignedOffset + size)
    {
        ResizeBuffer(alignedOffset + size);
    }

    void* ptr = buffer.Data() + alignedOffset;

    bufferOffset = alignedOffset + size;

    outHeader = {};
    outHeader.offset = alignedOffset;
    outHeader.size = size;

    return ptr;
}

void SafeDeleter::EntryList::Push(const EntryHeader& header)
{
    currHeaders->PushBack(header);
}

void SafeDeleter::EntryList::ResizeBuffer(SizeType newMinSize)
{
    ByteBuffer newBuffer;
    newBuffer.SetSize(MathUtil::Ceil<double, SizeType>(newMinSize * 1.5));

    // have to move all current commands since the buffer will realloc
    Array<EntryHeader>& headers = *currHeaders;
    SwapHeaderBuffers();

    for (EntryHeader& currHeader : headers)
    {
        if (currHeader.moveFn)
        {
            currHeader.moveFn(reinterpret_cast<void*>(newBuffer.Data() + currHeader.offset), reinterpret_cast<void*>(buffer.Data() + currHeader.offset));
        }
        else
        {
            Memory::MemCpy(newBuffer.Data() + currHeader.offset, buffer.Data() + currHeader.offset, currHeader.size);
        }

        if (currHeader.destructFn)
        {
            currHeader.destructFn(reinterpret_cast<void*>(buffer.Data() + currHeader.offset));
        }
    }

    // concat back the headers that were added while iterating
    headers.Concat(std::move(*currHeaders));
    currHeaders = &headers;

    buffer = std::move(newBuffer);
}

} // namespace hyperion
