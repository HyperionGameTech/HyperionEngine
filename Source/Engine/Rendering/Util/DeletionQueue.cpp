/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/RenderInterface.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Scripting/ScriptObjectResource.hpp>

namespace Hyperion {

DeletionQueue& DeletionQueue::GetInstance()
{
    static DeletionQueue s_instance;
    return s_instance;
}

#pragma region DeletionQueueElem<Handle<ObjectBase>>

DeletionQueueElem<Handle<ObjectBase>>::DeletionQueueElem(ObjectBase* ptr)
    : ptr(ptr)
{
//    if (ptr)
//    {
//#ifdef HYP_DOTNET
//        ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();
//        const bool hasExtraRef = scriptObjectResource && bool(scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)));
//#else
//        const bool hasExtraRef = false;
//#endif
//        ObjectHeader* header = ptr->GetObjectHeader_Internal();
//
//        int32 currentCount = AtomicIncrement(&header->refCountStrong);
//        AssertDebug(currentCount > 1); // should have another ref from input
//
//        while (currentCount != 1 + (hasExtraRef ? 1 : 0))
//        {
//            if (AtomicCompareExchange(&header->refCountStrong, currentCount, currentCount - 1))
//            {
//
//#ifdef HYP_DOTNET
//                if (hasExtraRef)
//                {
//                    Object_DecScriptObjectRef(ptr);
//                }
//#endif
//
//                break;
//            }
//        }
//    }
}

void DeletionQueueElem<Handle<ObjectBase>>::DestroyObject()
{
    // call destructor if no more strong references
    if (ptr)
    {
        ObjectHeader* header = ptr->GetObjectHeader_Internal();

        /// @NOTE: Objects with C# scripts that haven't been GC'd here wouldn't get deleted.
        /// However, we incremented the strong ref count in the constructor to prevent deletion until this point.
        /// So the object would've been kept alive long enough to be safe to use during rendering.
        /// When the .NET GC runs, it will decrement the strong ref count and delete the object immediately if it reaches 0.
        const int32 count = AtomicDecrement(&header->refCountStrong);

        if (count == 0)
        {
            // we increment weak reference to prevent weak refs to this from causing Release() upon calling their destructors.
            header->IncRefWeak();

            ptr->~ObjectBase();

            // this will free the slot if no other weak references remain
            header->DecRefWeak();
        }
        else
        {
            AssertDebug(count >= 0);

#ifdef HYP_DOTNET
            Object_DecScriptObjectRef(ptr);
#endif
        }
    }
}

#pragma region DeletionQueueElem<Handle<ObjectBase>>

#pragma region DeletionQueue

DeletionQueue::DeletionQueue()
    : m_entryLists { nullptr },
      m_tempEntryListCount(0)
{
    for (uint32 i = 0; i < RingBufferDepth; i++)
    {
        m_entryLists[i] = new DeletionQueue::EntryList<DynamicAllocator>();
    }
}

DeletionQueue::~DeletionQueue() = default;

void DeletionQueue::Shutdown()
{
    AssertOnThread(g_renderThread);

    const uint32 bufferIndex = GetRingIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& currentEntryList = *m_entryLists[bufferIndex];

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
                Memory::Move(currentEntryList.buffer.Data() + newAlignedOffset, vp, header.size);
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

    // delete remaining enqueued deletions
    FixedArray<int, RingBufferDepth> counts {};

    do
    {
        for (uint32 i = 0; i < RingBufferDepth; i++)
        {
            counts[i] = ForceDeleteAll(i);
        }

        ThreadSleep(1); // give some time for other threads to finish
    }
    while (AnyOf(counts, [](uint32 count) { return count > 0; }));

    // delete all entries in all buffers
    for (auto* pEntryList : m_entryLists)
    {
        Assert(pEntryList->headers->Empty());

        delete pEntryList;
    }
}

void DeletionQueue::GetCounterValues(uint32& outNumElements, uint32& outTotalBytes) const
{
    AssertOnThread(g_renderThread);

    outNumElements = 0;
    outTotalBytes = 0;

    for (const Counter& counter : m_counters)
    {
        outNumElements += counter.numElements;
        outTotalBytes += counter.numTotalBytes;
    }
}

int DeletionQueue::Iterate(int maxIter)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    uint32 bufferIndex = GetRingIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& entryList = *m_entryLists[bufferIndex];

    Array<EntryHeader>& headers = *entryList.currHeaders;
    entryList.SwapHeaderBuffers();

    const uint32 frameCounter = GetFrameCounter();

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

        for (size_t headerIdx = 0; headerIdx < headers.Size(); headerIdx++)
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

size_t DeletionQueue::ForceDeleteAll(uint32 bufferIndex)
{
    AssertOnThread(g_renderThread);

    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& entryList = *m_entryLists[bufferIndex];

    AssertDebug(entryList.currHeaders == &entryList.headers[0]);
    AssertDebug(entryList.headers[1].Empty());

    size_t numTotalDestroyed = 0;

    while (!entryList.currHeaders->Empty())
    {
        size_t currOffset = 0;
        size_t currCount = entryList.currHeaders->Size();

        auto& prevHeaders = *entryList.currHeaders;
        EntryHeader* first = &(*entryList.currHeaders)[currOffset];

        // swap before destroying anything.
        entryList.SwapHeaderBuffers();

        for (; currOffset < currCount; currOffset++)
        {
            EntryHeader& header = *(first + currOffset);

            if (header.destructFn)
            {
                // swap while we force destruct
                header.destructFn(reinterpret_cast<void*>(entryList.buffer.Data() + header.offset));
            }
        }

        AssertDebug(prevHeaders.Size() == currOffset);

        numTotalDestroyed += prevHeaders.Size();

        prevHeaders.Clear();
    }

    // Reset headers back to headers[0] after all are cleared
    entryList.currHeaders = &entryList.headers[0];

    // clear buffer if all entries have been deleted
    entryList.buffer.Clear();
    entryList.bufferPos = 0;

    return numTotalDestroyed;
}

void DeletionQueue::UpdateCounter(uint32 bufferIndex)
{
    AssertOnThread(g_renderThread);

    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& entryList = *m_entryLists[bufferIndex];
    Counter& counter = m_counters[bufferIndex];

    counter.numElements = entryList.currHeaders->Size();
    counter.numTotalBytes = entryList.buffer.Size();
}

void DeletionQueue::Flush()
{
    AssertOnThread(g_renderThread);

    uint32 bufferIndex = GetRingIndex();
    ForceDeleteAll(bufferIndex);
}

void DeletionQueue::UpdateEntryListQueue()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    uint32 bufferIndex = GetRingIndex();
    AssertDebug(bufferIndex < m_entryLists.Size());

    auto& currentEntryList = *m_entryLists[bufferIndex];

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
                Memory::Move(currentEntryList.buffer.Data() + newAlignedOffset, vp, header.size);
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

#pragma endregion DeletionQueue

#pragma region DeletionQueue::EntryList

DeletionQueue::EntryListBase& DeletionQueue::GetCurrentEntryList(Mutex::Guard** ppGuard)
{
    AssertDebug(ppGuard != nullptr);
    *ppGuard = nullptr;

    if (IsOnThread(g_simThread | g_renderThread))
    {
        uint32 bufferIndex = GetRingIndex();
        AssertDebug(bufferIndex < m_entryLists.Size());

        return *m_entryLists[bufferIndex];
    }

    *ppGuard = new Mutex::Guard(m_mutex);

    AtomicIncrement(&m_tempEntryListCount);
    auto& entryList = m_tempEntryLists.EmplaceBack();

    return entryList;
}

DeletionQueue::EntryListBase& DeletionQueue::GetEntryList(Mutex::Guard** ppGuard, uint32 desiredIdx)
{
    // If:
    //  - desiredIdx == ~0u (not specified) OR
    //  - On render thread and desiredIdx == CURRENT idx
    // we use the CURRENT entry list
    if (desiredIdx == ~0u || (IsOnThread(g_renderThread) && desiredIdx == GetRingIndex()))
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

#pragma endregion DeletionQueue::EntryList

} // namespace Hyperion
