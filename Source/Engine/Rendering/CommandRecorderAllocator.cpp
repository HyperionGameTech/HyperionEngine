/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/CommandRecorderAllocator.hpp>

#include <Rendering/RenderInterface.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

namespace Hyperion {

#pragma region CommandRecorderAllocator

CommandRecorderAllocator::CommandRecorderAllocator()
    : m_tempCommandRecordersCount(0),
      m_isShuttingDown(false)
{
}

CommandRecorderAllocator::~CommandRecorderAllocator() = default;

void CommandRecorderAllocator::Shutdown()
{
    AssertOnThread(g_renderThread);

    Mutex::Guard guard(m_mutex);

    AtomicAdd(&m_tempCommandRecordersCount, -int32(m_tempPreRenderCommandRecorders.Size() + m_tempCommandRecorders.Size()));

    m_tempPreRenderCommandRecorders.Clear();
    m_tempCommandRecorders.Clear();

    rootPreRender.Reset(/* freeMemory */ true);
    root.Reset(/* freeMemory */ true);
}

void CommandRecorderAllocator::Flush(bool isShuttingDown)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Mutex::Guard guard(m_mutex);

    if (m_isShuttingDown)
    {
        return;
    }

    m_isShuttingDown = true;

    UpdateQueue_Internal();

    rootPreRender.Submit();
    root.Submit();
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

    UpdateQueue_Internal();
}

void CommandRecorderAllocator::UpdateQueue_Internal()
{
    DrainTempCommandRecorders(m_tempPreRenderCommandRecorders, rootPreRender);
    DrainTempCommandRecorders(m_tempCommandRecorders, root);
}

void CommandRecorderAllocator::DrainTempCommandRecorders(List<CommandRecorder>& tempCommandRecorders, CommandRecorder& dst)
{
    for (auto it = tempCommandRecorders.Begin(); it != tempCommandRecorders.End();)
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
            it = tempCommandRecorders.Erase(it);
            AtomicDecrement(&m_tempCommandRecordersCount);

            continue;
        }

        // Concat to the destination root (only happens if the Submit() method hasn't been called)
        dst.Concat(commandRecorder);

        // commandRecorder is now reset

        it = tempCommandRecorders.Erase(it);

        AtomicDecrement(&m_tempCommandRecordersCount);
    }
}

CommandRecorder& CommandRecorderAllocator::GetCommandRecorder(CommandRecorderQueue queue)
{
    Mutex::Guard guard(m_mutex);

    AssertDebug(!m_isShuttingDown);

    List<CommandRecorder>& tempCommandRecorders = queue == CommandRecorderQueue::PreRender
        ? m_tempPreRenderCommandRecorders
        : m_tempCommandRecorders;

    AtomicIncrement(&m_tempCommandRecordersCount);
    auto& newCommandRecorder = tempCommandRecorders.EmplaceBack();

    AssertDebug(newCommandRecorder.IsWritable());

    return newCommandRecorder;
}

#pragma endregion DeletionQueue

} // namespace Hyperion
