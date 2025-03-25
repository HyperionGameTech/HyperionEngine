/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RenderCommand.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {
namespace renderer {

RenderCommands::Buffer RenderCommands::buffers[2] = { };
AtomicVar<uint32> RenderCommands::buffer_index = 0;
RenderCommands::RenderCommandSemaphore RenderCommands::semaphore = { };

// // Note: double buffering is currently disabled as it is causing some issues with textures not being
// // initalized when they are first added to a material's descriptor set.
// #define HYP_RENDER_COMMANDS_DOUBLE_BUFFERED

#pragma region RenderScheduler

void RenderScheduler::Commit(RenderCommand *command)
{
    m_commands.PushBack(command);
}

void RenderScheduler::AcceptAll(Array<RenderCommand *> &out_container)
{
    out_container = std::move(m_commands);
}

#pragma endregion RenderScheduler

#pragma region RenderCommands

void RenderCommands::PushCustomRenderCommand(RENDER_COMMAND(CustomRenderCommand) *command)
{
    Buffer &buffer = buffers[CurrentBufferIndex()];

    buffer.scheduler.m_num_enqueued.Increment(1, MemoryOrder::RELEASE);

    std::unique_lock lock(buffer.mtx);

    buffer.scheduler.Commit(command);
}

RendererResult RenderCommands::Flush()
{
    HYP_NAMED_SCOPE("Flush render commands");

    Threads::AssertOnThread(g_render_thread);

    Array<RenderCommand *> commands;

#ifdef HYP_RENDER_COMMANDS_DOUBLE_BUFFERED
    const uint32 buffer_index = RenderCommands::buffer_index.Increment(1, MemoryOrder::ACQUIRE_RELEASE) % 2;
    Buffer &buffer = buffers[buffer_index];

    // Lock in order to accept commands, so when
    // one of our render commands pushes to the queue,
    // it will not cause a deadlock.
    // Also it will be more performant this way as less time will be spent
    // in the locked section.
    std::unique_lock lock(buffer.mtx);

    buffer.scheduler.AcceptAll(commands);
    buffer.scheduler.m_num_enqueued.Decrement(commands.Size(), MemoryOrder::RELEASE);

    lock.unlock();

    const SizeType num_commands = commands.Size();

    for (SizeType index = 0; index < num_commands; index++) {
        RenderCommand *front = commands[index];

        HYP_LOG(RenderCommands, Debug, "Executing render command {} on buffer {}", front->_debug_name, buffer_index);

        const RendererResult command_result = front->Call();
        AssertThrowMsg(command_result, "Render command error! [%d]: %s\n", command_result.GetError().GetErrorCode(), command_result.GetError().GetMessage().Data());
        front->~RenderCommand();
    }

    if (num_commands) {
        // Buffer is swapped now, safe to rewind without locking mutex.
        Rewind(buffer_index);
    }
#else
    const uint32 buffer_index = 0;
    Buffer &buffer = buffers[buffer_index];

    std::unique_lock lock(buffer.mtx);

    buffer.scheduler.AcceptAll(commands);

    const SizeType num_commands = commands.Size();

    for (SizeType index = 0; index < num_commands; index++) {
        RenderCommand *front = commands[index];

#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
        HYP_NAMED_SCOPE(front->_debug_name);
#else
        HYP_NAMED_SCOPE("Executing render command");
#endif

        const RendererResult command_result = front->Call();
        AssertThrowMsg(command_result, "Render command error! [%d]: %s\n", command_result.GetError().GetErrorCode(), command_result.GetError().GetMessage().Data());
        
        front->~RenderCommand();
    }

    if (num_commands) {
        Rewind(buffer_index);
    }

    buffer.scheduler.m_num_enqueued.Decrement(num_commands, MemoryOrder::RELEASE);

    lock.unlock();
#endif

    semaphore.Produce(1);

    return { };
}

void RenderCommands::Wait()
{
    HYP_SCOPE;

    Threads::AssertOnThread(~g_render_thread);

    const uint32 current_value = semaphore.GetValue();

    // wait for the counter to increment by 1
    semaphore.WaitForValue(current_value + 1);
}

void RenderCommands::Rewind(uint32 buffer_index)
{
#ifdef HYP_RENDER_COMMANDS_DOUBLE_BUFFERED
    AssertDebugMsg(buffer_index != CurrentBufferIndex(), "cannot rewind the current buffer");
#endif

    // all items in the cache must have had destructor called on them already.
    Buffer &buffer = buffers[buffer_index];

    for (auto it = buffer.holders.Begin(); it != buffer.holders.End(); ++it) {
        if (!it->render_command_list_ptr) {
            break;
        }

        it->rewind_func(it->render_command_list_ptr, buffer_index);
    }
}

#pragma endregion RenderCommands

} // namespace renderer
} // namespace hyperion