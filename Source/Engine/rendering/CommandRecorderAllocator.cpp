/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/CommandRecorderAllocator.hpp>

#include <rendering/RenderInterface.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/AtomicVar.hpp>

#include <Core/memory/allocator/Allocator.hpp>

namespace Hyperion {

#pragma region CommandRecorderAllocator

CommandRecorderAllocator::CommandRecorderAllocator()
    : m_tempCommandRecordersCount(0)
{
}

CommandRecorderAllocator::~CommandRecorderAllocator() = default;

void CommandRecorderAllocator::Shutdown()
{
    AssertOnThread(g_renderThread);

    Mutex::Guard guard(m_mutex);

    for (auto it = m_tempCommandRecorders.Begin(); it != m_tempCommandRecorders.End();)
    {
        auto& commandRecorder = *it;

        if (commandRecorder.IsEmpty())
        {
            // no data in buffers, skip
            it = m_tempCommandRecorders.Erase(it);
            AtomicDecrement(&m_tempCommandRecordersCount);

            continue;
        }

        it = m_tempCommandRecorders.Erase(it);

        AtomicDecrement(&m_tempCommandRecordersCount);
    }

    m_renderThreadCommandRecorder.Reset(/* freeMemory */ true);
}

void CommandRecorderAllocator::UpdateQueue()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (AtomicAdd(&m_tempCommandRecordersCount, 0) == 0)
    {
        // no temp recorders, nothing to concat to ours
        return;
    }

    Mutex::Guard guard(m_mutex);

    for (auto it = m_tempCommandRecorders.Begin(); it != m_tempCommandRecorders.End();)
    {
        auto& commandRecorder = *it;

        // only destroy/read if not in writable state (other thread could be using it)
        if (commandRecorder.IsWritable())
        {
            ++it;
            continue;
        }

        if (commandRecorder.IsEmpty())
        {
            // no data in buffers, skip
            it = m_tempCommandRecorders.Erase(it);
            AtomicDecrement(&m_tempCommandRecordersCount);

            continue;
        }

        m_renderThreadCommandRecorder.Concat(commandRecorder);

        // commandRecorder is now reset

        it = m_tempCommandRecorders.Erase(it);

        AtomicDecrement(&m_tempCommandRecordersCount);
    }
}

CommandRecorder& CommandRecorderAllocator::GetCommandRecorder()
{
    if (IsOnThread(g_renderThread))
    {
        ++m_renderThreadCommandRecorder.writeCount;
        return m_renderThreadCommandRecorder;
    }

    Mutex::Guard guard(m_mutex);

    AtomicIncrement(&m_tempCommandRecordersCount);
    auto& newCommandRecorder = m_tempCommandRecorders.EmplaceBack();

    AssertDebug(newCommandRecorder.IsWritable());

    return newCommandRecorder;
}

#pragma endregion DeletionQueue

} // namespace Hyperion
