/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCommand.hpp>
#include <rendering/RenderBackend.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {
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

void RenderScheduler::AcceptAll(Array<RenderCommand*>& outContainer)
{
    outContainer = std::move(m_commands);
}

#pragma endregion RenderScheduler

#pragma region RenderCommands

void RenderCommands::PushCustomRenderCommand(CustomRenderCommand* command)
{
    Buffer& buffer = s_buffers[CurrentBufferIndex()];

    buffer.scheduler.m_numEnqueued.Increment(1, MemoryOrder::RELEASE);

    std::unique_lock lock(buffer.mtx);

    buffer.scheduler.Commit(command);
}

RendererResult RenderCommands::Flush()
{
    HYP_NAMED_SCOPE("Flush render commands");

    Threads::AssertOnThread(g_renderThread);

    Array<RenderCommand*> commands;

#ifdef HYP_RENDER_COMMANDS_DOUBLE_BUFFERED
    const uint32 bufferIndex = RenderCommands::s_bufferIndex.Increment(1, MemoryOrder::ACQUIRE_RELEASE) % 2;
    Buffer& buffer = s_buffers[bufferIndex];

    std::unique_lock lock(buffer.mtx);

    buffer.scheduler.AcceptAll(commands);

    const SizeType numCommands = commands.Size();

    for (SizeType index = 0; index < numCommands; index++)
    {
        RenderCommand* front = commands[index];

#ifdef HYP_RENDER_COMMANDS_DEBUG_LOG_NAME
        HYP_LOG(RenderCommands, Debug, "Executing render command {} on buffer {}", front->_debug_name, bufferIndex);
#endif

        const RendererResult commandResult = front->Call();
        HYP_GFX_ASSERT(commandResult, "Render command error! [%d]: %s\n", commandResult.GetError().GetErrorCode(), commandResult.GetError().GetMessage());
        front->~RenderCommand();
    }

    if (numCommands)
    {
        buffer.scheduler.m_numEnqueued.Decrement(numCommands, MemoryOrder::RELEASE);

        Rewind(bufferIndex);
    }

    lock.unlock();
#else
    const uint32 bufferIndex = 0;
    Buffer& buffer = s_buffers[bufferIndex];

    std::unique_lock lock(buffer.mtx);

    buffer.scheduler.AcceptAll(commands);

    const SizeType numCommands = commands.Size();

    for (SizeType index = 0; index < numCommands; index++)
    {
        RenderCommand* front = commands[index];

#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
        HYP_NAMED_SCOPE(front->_debug_name);
#else
        HYP_NAMED_SCOPE("Executing render command");
#endif

#ifdef HYP_RENDER_COMMANDS_DEBUG_LOG_NAME
        HYP_LOG(RenderCommands, Debug, "Executing render command {} on buffer {}", front->_debug_name, bufferIndex);
#endif

        const RendererResult commandResult = front->Call();
        HYP_GFX_ASSERT(commandResult, "Render command error! [%d]: %s\n", commandResult.GetError().GetErrorCode(), commandResult.GetError().GetMessage());

        front->~RenderCommand();
    }

    if (numCommands)
    {
        Rewind(bufferIndex);
    }

    buffer.scheduler.m_numEnqueued.Decrement(numCommands, MemoryOrder::RELEASE);

    lock.unlock();
#endif

    s_semaphore.Produce(1);

    return {};
}

void RenderCommands::Wait()
{
    HYP_SCOPE;

    Threads::AssertOnThread(~g_renderThread);

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

} // namespace hyperion