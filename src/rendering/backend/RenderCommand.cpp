/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RenderCommand.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {
namespace renderer {

FixedArray<RenderCommandHolder, max_render_command_types> RenderCommands::holders = { };
AtomicVar<SizeType> RenderCommands::render_command_type_index = { 0 };
RenderScheduler RenderCommands::scheduler = { };

std::mutex RenderCommands::mtx = std::mutex();
std::condition_variable RenderCommands::flushed_cv = std::condition_variable();

uint RenderCommands::buffer_index = 0;

// // Note: double buffering is currently disabled as it is causing some issues with textures not being
// // initalized when they are first added to a material's descriptor set.
// #define HYP_RENDER_COMMANDS_DOUBLE_BUFFERED

#pragma region RenderScheduler

void RenderScheduler::Commit(RenderCommand *command)
{
    m_commands.PushBack(command);
    m_num_enqueued.Increment(1, MemoryOrder::RELEASE);
}

void RenderScheduler::AcceptAll(Array<RenderCommand *> &out_container)
{
    out_container = std::move(m_commands);
    m_num_enqueued.Set(0, MemoryOrder::RELEASE);
}

#pragma endregion RenderScheduler

#pragma region RenderCommands

void RenderCommands::PushCustomRenderCommand(RENDER_COMMAND(CustomRenderCommand) *command)
{
    std::unique_lock lock(mtx);

    scheduler.Commit(command);
}

Result RenderCommands::Flush()
{
    if (Count() == 0) {
        HYPERION_RETURN_OK;
    }

    HYP_NAMED_SCOPE("Flush render commands");

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    Array<RenderCommand *> commands;

#ifdef HYP_RENDER_COMMANDS_DOUBLE_BUFFERED

    // Lock in order to accept commands, so when
    // one of our render commands pushes to the queue,
    // it will not cause a deadlock.
    // Also it will be more performant this way as less time will be spent
    // in the locked section.
    std::unique_lock lock(mtx);

    const uint buffer_index = RenderCommands::buffer_index;

    scheduler.AcceptAll(commands);

    const SizeType num_commands = commands.Size();

    // Swap buffers before executing commands, so that the commands may enqueue new commands
    SwapBuffers();

    lock.unlock();

    flushed_cv.notify_all();

    for (SizeType index = 0; index < num_commands; index++) {
        RenderCommand *front = commands[index];

        const Result command_result = front->Call();
        AssertThrowMsg(command_result, "Render command error! [%d]: %s\n", command_result.error_code, command_result.message);
        front->~RenderCommand();
    }

    AssertThrowMsg(((buffer_index + 1) & 1) == RenderCommands::buffer_index, "Buffer index mismatch! %u != %u", buffer_index, RenderCommands::buffer_index);

    if (num_commands) {
        // Use previous buffer index since we call SwapBuffers before executing commands.
        Rewind((buffer_index + 1) & 1);
    }
#else
    std::unique_lock lock(mtx);

    scheduler.AcceptAll(commands);

    const SizeType num_commands = commands.Size();

    for (SizeType index = 0; index < num_commands; index++) {
        RenderCommand *front = commands[index];

#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
        HYP_NAMED_SCOPE(front->_debug_name);
#else
        HYP_NAMED_SCOPE("Executing render command");
#endif

        const Result command_result = front->Call();
        AssertThrowMsg(command_result, "Render command error! [%d]: %s\n", command_result.error_code, command_result.message);
        front->~RenderCommand();
    }

    if (num_commands) {
        Rewind(buffer_index);
    }

    lock.unlock();

    flushed_cv.notify_all();
#endif

    return Result { };
}

Result RenderCommands::FlushOrWait()
{
    HYP_SCOPE;

    if (Count() == 0) {
        HYPERION_RETURN_OK;
    }

    if (Threads::IsOnThread(ThreadName::THREAD_RENDER)) {
        return Flush();
    }

    Wait();

    HYPERION_RETURN_OK;
}

void RenderCommands::Wait()
{
    HYP_SCOPE;

    if (Count() == 0) {
        return;
    }

    Threads::AssertOnThread(~ThreadName::THREAD_RENDER);

    std::unique_lock lock(mtx);
    flushed_cv.wait(lock, [] { return RenderCommands::Count() == 0; });
}

void RenderCommands::SwapBuffers()
{
    buffer_index = (buffer_index + 1) & 1;
}

void RenderCommands::Rewind(uint buffer_index)
{
    // all items in the cache must have had destructor called on them already.

    for (auto it = holders.Begin(); it != holders.End(); ++it) {
        if (!it->render_command_list_ptr) {
            break;
        }

        it->rewind_func(it->render_command_list_ptr, buffer_index);
    }
}

#pragma endregion RenderCommands

} // namespace renderer
} // namespace hyperion