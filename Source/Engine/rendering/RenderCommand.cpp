/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/RenderInterface.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/logging/LogChannels.hpp>
#include <Core/logging/Logger.hpp>

#include <Core/profiling/ProfileScope.hpp>

namespace Hyperion {
RenderCommands::Buffer RenderCommands::s_buffers[2] = {};
AtomicVar<uint32> RenderCommands::s_bufferIndex = 0;
RenderCommands::RenderCommandSemaphore RenderCommands::s_semaphore = {};

#define HYP_RENDER_COMMANDS_DOUBLE_BUFFERED
// #define HYP_RENDER_COMMANDS_DEBUG_LOG_NAME

#pragma region RenderScheduler

void RenderScheduler::Commit(RenderCommand* command)
{
    m_commands.PushBack(command);
}

void RenderScheduler::AcceptAll(Array<RenderCommand*, RenderAllocator>& outContainer)
{
    outContainer.Concat(std::move(m_commands));
    m_commands.Clear();
}

#pragma endregion RenderScheduler

#pragma region RenderCommands

void RenderCommands::PushCustomRenderCommand(CustomRenderCommand* command)
{
    Buffer& buffer = s_buffers[CurrentBufferIndex()];

    buffer.scheduler.m_numEnqueued.Increment(1, MemoryOrder::RELEASE);

    Mutex::Guard guard(buffer.mtx);
    buffer.scheduler.Commit(command);
}

void RenderCommands::Flush()
{
    HYP_NAMED_SCOPE("Flush render commands");

    AssertOnThread(g_renderThread);

    Array<RenderCommand*, RenderAllocator> commands;

#ifdef HYP_RENDER_COMMANDS_DOUBLE_BUFFERED
    const uint32 bufferIndex = RenderCommands::s_bufferIndex.Increment(1, MemoryOrder::ACQUIRE_RELEASE) % 2;
    Buffer& buffer = s_buffers[bufferIndex];

    buffer.mtx.Lock();

    buffer.scheduler.AcceptAll(commands);

    const size_t numCommands = commands.Size();

    for (size_t index = 0; index < numCommands; index++)
    {
        RenderCommand* front = commands[index];

#ifdef HYP_RENDER_COMMANDS_DEBUG_LOG_NAME
        HYP_LOG(RenderCommands, Verbose, "Executing render command {} on buffer {}", front->_debug_name, bufferIndex);
#endif

        CheckResult(front->Call());
        front->~RenderCommand();
    }

    if (numCommands)
    {
        buffer.scheduler.m_numEnqueued.Decrement(numCommands, MemoryOrder::RELEASE);

        Rewind(bufferIndex);
    }

    buffer.mtx.Unlock();
#else
    const uint32 bufferIndex = 0;
    Buffer& buffer = s_buffers[bufferIndex];

    buffer.mtx.Lock();

    buffer.scheduler.AcceptAll(commands);

    const size_t numCommands = commands.Size();

    for (size_t index = 0; index < numCommands; index++)
    {
        RenderCommand* front = commands[index];

#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
        HYP_NAMED_SCOPE(front->_debug_name);
#else
        HYP_NAMED_SCOPE("Executing render command");
#endif

#ifdef HYP_RENDER_COMMANDS_DEBUG_LOG_NAME
        HYP_LOG(RenderCommands, Verbose, "Executing render command {} on buffer {}", front->_debug_name, bufferIndex);
#endif

        CheckResult(front->Call());

        front->~RenderCommand();
    }

    if (numCommands)
    {
        Rewind(bufferIndex);
    }

    buffer.scheduler.m_numEnqueued.Decrement(numCommands, MemoryOrder::RELEASE);

    buffer.mtx.Unlock();
#endif

    s_semaphore.Produce(1);
}

void RenderCommands::Wait()
{
    HYP_SCOPE;

    AssertOnThread(~g_renderThread);

    const uint32 currentValue = s_semaphore.GetValue();

#ifdef HYP_RENDER_COMMANDS_DOUBLE_BUFFERED
    s_semaphore.WaitForValue(currentValue + 2);
#else
    s_semaphore.WaitForValue(currentValue + 1);
#endif
}

void RenderCommands::Rewind(uint32 bufferIndex)
{
    // all items in the cache must have had destructor called on them already.
    Buffer& buffer = s_buffers[bufferIndex];

    for (auto it = buffer.holders.Begin(); it != buffer.holders.End(); ++it)
    {
        if (!it->renderCommandListPtr)
        {
            break;
        }

        it->rewindFunc(it->renderCommandListPtr, bufferIndex);
    }
}

#pragma endregion RenderCommands

} // namespace Hyperion
