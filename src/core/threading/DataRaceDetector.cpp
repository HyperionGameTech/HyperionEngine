/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Bitset.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>

namespace hyperion {
namespace threading {

#ifdef HYP_ENABLE_MT_CHECK

#pragma region DataAccessScope

DataRaceDetector::DataAccessScope::DataAccessScope(EnumFlags<DataAccessFlags> flags, const DataRaceDetector& detector, const DataAccessState& state)
    : m_detector(const_cast<DataRaceDetector&>(detector)),
      m_threadId(ThreadId::Current())
{
    // Check if we should suppress data race detection
    if (IsGlobalContextActive<SuppressDataRaceDetectorContext>())
    {
        m_flags = 0;

        return;
    }

    m_flags = m_detector.AddAccess(m_threadId, flags, state);
}

DataRaceDetector::DataAccessScope::~DataAccessScope()
{
    m_detector.RemoveAccess(m_threadId, m_flags);
}

#pragma endregion DataAccessScope

#pragma region DataRaceDetector

static Span<const DataRaceDetector::ThreadAccessState> GetPreallocatedStates()
{
    static const struct PreallocatedThreadAccessStates
    {
        FixedArray<DataRaceDetector::ThreadAccessState, DataRaceDetector::numPreallocatedStates> data;

        PreallocatedThreadAccessStates()
        {
            for (SizeType i = 0; i < DataRaceDetector::numPreallocatedStates; i++)
            {
                const StaticThreadId threadId = StaticThreadId(i);

                data[i] = DataRaceDetector::ThreadAccessState { threadId, DataAccessFlags::ACCESS_NONE };
            }
        }
    } preallocatedStates {};

    return preallocatedStates.data;
}

DataRaceDetector::DataRaceDetector()
    : m_preallocatedStates(GetPreallocatedStates()),
      m_writers(0),
      m_readers(0)
{
    HYP_CORE_ASSERT(m_preallocatedStates.Size() == numPreallocatedStates);
}

DataRaceDetector::~DataRaceDetector()
{
}

EnumFlags<DataAccessFlags> DataRaceDetector::AddAccess(ThreadId threadId, EnumFlags<DataAccessFlags> accessFlags, const DataAccessState& state)
{
    Threads::AssertOnThread(threadId);

    uint32 index = ~0u;

    if (threadId.IsStatic())
    {
        // ensure no writer from other thread
        index = static_cast<const StaticThreadId&>(threadId).GetStaticThreadIndex();
        HYP_CORE_ASSERT(index < numPreallocatedStates);

        // disable bits that are already enabled
        accessFlags &= ~m_preallocatedStates[index].access;

        // If no access flags will be changed, return now (don't overwrite state)
        if (accessFlags == DataAccessFlags::ACCESS_NONE)
        {
            return DataAccessFlags::ACCESS_NONE;
        }

        m_preallocatedStates[index].access |= accessFlags;
        m_preallocatedStates[index].state = state;
    }
    else
    {
        Mutex::Guard guard(m_dynamicStatesMutex);

        // debugging
        HYP_CORE_ASSERT(m_dynamicStates.Size() <= m_dynamicStates.Capacity());

        auto it = m_dynamicStates.FindIf([threadId](const ThreadAccessState& threadAccessState)
            {
                return threadAccessState.threadId == threadId;
            });

        if (it == m_dynamicStates.End())
        {
            ThreadAccessState& threadAccessState = m_dynamicStates.EmplaceBack();
            threadAccessState.threadId = threadId;
            threadAccessState.originalIndex = uint32(m_dynamicStates.Size() - 1);

            it = &threadAccessState;
        }

        // disable bits that are already enabled
        accessFlags &= ~it->access;

        // If no access flags will be changed, return now (don't overwrite state)
        if (accessFlags == DataAccessFlags::ACCESS_NONE)
        {
            return DataAccessFlags::ACCESS_NONE;
        }

        it->access |= accessFlags;
        it->state = state;

        index = (it - m_dynamicStates.Begin()) + numPreallocatedStates;
    }

    if (index == ~0u)
    {
        return DataAccessFlags::ACCESS_NONE;
    }

    const uint64 testMask = 1ull << index;

    uint64 mask;

    if (accessFlags & DataAccessFlags::ACCESS_WRITE)
    {
        if (((mask = m_writers.BitOr(testMask, MemoryOrder::ACQUIRE_RELEASE)) & ~testMask))
        {
            LogDataRace(0ull, mask);
            HYP_FAIL("Potential data race detected: Attempt to acquire write access while other thread is writing. Write mask: {}", mask);
        }

        if ((mask = m_readers.BitOr(testMask, MemoryOrder::ACQUIRE_RELEASE)) & ~testMask)
        {
            LogDataRace(mask, 0ull);
            HYP_FAIL("Potential data race detected: Attempt to acquire write access while other thread is reading. Write mask: {}", mask);
        }
    }
    else
    {
        m_readers.BitOr(testMask, MemoryOrder::RELEASE);

        if ((mask = m_writers.Get(MemoryOrder::ACQUIRE)) & ~testMask)
        {
            LogDataRace(mask, 0ull);
            HYP_FAIL("Potential data race detected: Attempt to acquire read access while other thread is writing. Write mask: {}", mask);
        }
    }

    // return the new bits that were enabled, so scopes know which ones to disable on exit
    return accessFlags;
}

void DataRaceDetector::RemoveAccess(ThreadId threadId, EnumFlags<DataAccessFlags> accessFlags)
{
    if (accessFlags == DataAccessFlags::ACCESS_NONE)
    {
        return;
    }

    Threads::AssertOnThread(threadId);

    uint32 index = ~0u;
    EnumFlags<DataAccessFlags> flags = DataAccessFlags::ACCESS_NONE;

    if (threadId.IsStatic())
    {
        // ensure no writer from other thread
        index = static_cast<const StaticThreadId&>(threadId).GetStaticThreadIndex();
        HYP_CORE_ASSERT(index < numPreallocatedStates);

        flags = m_preallocatedStates[index].access;
        m_preallocatedStates[index].access &= ~accessFlags;
    }
    else
    {
        Mutex::Guard guard(m_dynamicStatesMutex);

        HYP_CORE_ASSERT(m_dynamicStates.Size() <= m_dynamicStates.Capacity());

        auto it = m_dynamicStates.FindIf([threadId](const ThreadAccessState& threadAccessState)
            {
                return threadAccessState.threadId == threadId;
            });

        if (it == m_dynamicStates.End())
        {
            return;
        }

        index = it->originalIndex + numPreallocatedStates;
        flags = it->access;

        // remove if no longer needed (all used flags removed)
        if (accessFlags == flags)
        {
            m_dynamicStates.Erase(it);
        }
    }

    if (index == ~0u)
    {
        return;
    }

    const uint64 testMask = 1ull << index;

    if (accessFlags & DataAccessFlags::ACCESS_READ)
    {
        m_readers.BitAnd(~testMask, MemoryOrder::RELEASE);
    }

    if (accessFlags & DataAccessFlags::ACCESS_WRITE)
    {
        m_writers.BitAnd(~testMask, MemoryOrder::RELEASE);
    }
}

void DataRaceDetector::LogDataRace(uint64 readersMask, uint64 writersMask) const
{
    Array<Pair<ThreadId, DataAccessState>> readerThreadIds;
    Array<Pair<ThreadId, DataAccessState>> writerThreadIds;
    GetThreadIds(readersMask, writersMask, readerThreadIds, writerThreadIds);

    String readerThreadsString = "<None>";

    if (readerThreadIds.Any())
    {
        readerThreadsString.Clear();

        for (SizeType i = 0; i < readerThreadIds.Size(); i++)
        {
            if (i == 0)
            {
                readerThreadsString = HYP_FORMAT("{} ({}) (at: {}, message: {})", readerThreadIds[i].first.GetName(), readerThreadIds[i].first.GetValue(), readerThreadIds[i].second.currentFunction, readerThreadIds[i].second.message);
            }
            else
            {
                readerThreadsString = HYP_FORMAT("{}, {} ({}) (at: {}, message: {})", readerThreadsString, readerThreadIds[i].first.GetName(), readerThreadIds[i].first.GetValue(), readerThreadIds[i].second.currentFunction, readerThreadIds[i].second.message);
            }
        }
    }

    String writerThreadsString = "<None>";

    if (writerThreadIds.Any())
    {
        writerThreadsString.Clear();

        for (SizeType i = 0; i < writerThreadIds.Size(); i++)
        {
            if (i == 0)
            {
                writerThreadsString = HYP_FORMAT("{} ({}) (at: {}, message: {})", writerThreadIds[i].first.GetName(), writerThreadIds[i].first.GetValue(), writerThreadIds[i].second.currentFunction, writerThreadIds[i].second.message);
            }
            else
            {
                writerThreadsString = HYP_FORMAT("{}, {} ({}) (at: {}, message: {})", writerThreadsString, writerThreadIds[i].first.GetName(), writerThreadIds[i].first.GetValue(), writerThreadIds[i].second.currentFunction, writerThreadIds[i].second.message);
            }
        }
    }

    HYP_LOG(DataRaceDetector, Error, "Data race detected: Current thread: {} ({}), Writer threads: {}, Reader threads: {}",
        ThreadId::Current().GetName(), ThreadId::Current().GetValue(),
        writerThreadsString,
        readerThreadsString);
    
    debug::DebugLog_FlushOutputStream();
}

void DataRaceDetector::GetThreadIds(uint64 readersMask, uint64 writersMask, Array<Pair<ThreadId, DataAccessState>>& outReaderThreadIds, Array<Pair<ThreadId, DataAccessState>>& outWriterThreadIds) const
{
    Mutex::Guard guard(m_dynamicStatesMutex);

    for (Bitset::BitIndex bitIndex : Bitset(readersMask))
    {
        if (bitIndex >= numPreallocatedStates)
        {
            const uint32 dynamicStateIndex = bitIndex - numPreallocatedStates;
            HYP_CORE_ASSERT(dynamicStateIndex < m_dynamicStates.Size(), "Invalid dynamic state index: {}; Out of range of elements: {}", dynamicStateIndex, m_dynamicStates.Size());

            outReaderThreadIds.EmplaceBack(m_dynamicStates[dynamicStateIndex].threadId, m_dynamicStates[dynamicStateIndex].state);
        }
        else
        {
            HYP_CORE_ASSERT(bitIndex < m_preallocatedStates.Size());

            outReaderThreadIds.EmplaceBack(m_preallocatedStates[bitIndex].threadId, m_preallocatedStates[bitIndex].state);
        }
    }

    for (Bitset::BitIndex bitIndex : Bitset(writersMask))
    {
        if (bitIndex >= numPreallocatedStates)
        {
            const uint32 dynamicStateIndex = bitIndex - numPreallocatedStates;
            HYP_CORE_ASSERT(dynamicStateIndex < m_dynamicStates.Size(), "Invalid dynamic state index: {}; Out of range of elements: {}", dynamicStateIndex, m_dynamicStates.Size());

            outWriterThreadIds.EmplaceBack(m_dynamicStates[dynamicStateIndex].threadId, m_dynamicStates[dynamicStateIndex].state);
        }
        else
        {
            HYP_CORE_ASSERT(bitIndex < m_preallocatedStates.Size());

            outWriterThreadIds.EmplaceBack(m_preallocatedStates[bitIndex].threadId, m_preallocatedStates[bitIndex].state);
        }
    }
}

#pragma endregion DataRaceDetector

#endif // HYP_ENABLE_MT_CHECK

} // namespace threading
} // namespace hyperion
