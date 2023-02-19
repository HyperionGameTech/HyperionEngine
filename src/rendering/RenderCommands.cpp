#include <rendering/RenderCommands.hpp>

namespace hyperion::v2 {

FixedArray<RenderCommandHolder, max_render_command_types> RenderCommands::holders = { };
AtomicVar<SizeType> RenderCommands::render_command_type_index = { 0 };
RenderScheduler RenderCommands::scheduler = { };

std::mutex RenderCommands::mtx = std::mutex();
std::condition_variable RenderCommands::flushed_cv = std::condition_variable();

void RenderScheduler::Commit(RenderCommand *command)
{
    m_commands.PushBack(command);
    m_num_enqueued.Increment(1, MemoryOrder::RELAXED);
}

RenderScheduler::FlushResult RenderScheduler::Flush()
{
    Threads::AssertOnThread(m_owner_thread);

    FlushResult result { Result::OK, 0 };

    SizeType index = 0;
    const SizeType num_commands = m_commands.Size();

    while (index < num_commands) {
        RenderCommand *front = m_commands[index++];

#ifdef HYP_DEBUG_LOG_RENDER_COMMANDS
        DebugLog(LogType::RenDebug, "Executing render command %s\n", front->_debug_name);
#endif

        result.result = front->Call();
        front->~RenderCommand();

        ++result.num_executed;

        AssertThrowMsg(result.result, "Render command error! %s\n", result.result.message);
    }

    m_commands.Clear();

    return result;
}

Result RenderCommands::Flush()
{
    if (Count() == 0) {
        HYPERION_RETURN_OK;
    }

    std::unique_lock lock(mtx);

    auto flush_result = scheduler.Flush();
    if (flush_result.num_executed) {
        Rewind();

        scheduler.m_num_enqueued.Decrement(flush_result.num_executed, MemoryOrder::RELAXED);
    }

    lock.unlock();
    flushed_cv.notify_all();

    return flush_result.result;
}

Result RenderCommands::FlushOrWait()
{
    if (Count() == 0) {
        HYPERION_RETURN_OK;
    }

    if (Threads::CurrentThreadID() == scheduler.m_owner_thread) {
        return Flush();
    }

    Wait();

    HYPERION_RETURN_OK;
}

void RenderCommands::Wait()
{
    std::unique_lock lock(mtx);
    flushed_cv.wait(lock, [] { return RenderCommands::Count() == 0; });
}

void RenderCommands::Rewind()
{
    // all items in the cache must have had destructor called on them already.

    for (auto it = holders.Begin(); it != holders.End(); ++it) {
        if (!it->render_command_list_ptr) {
            break;
        }

        it->rewind_func(it->render_command_list_ptr);
    }

#if 0
    RenderCommandHolder *p = holders.Data();

    while (p->render_command_list_ptr) {
        p->rewind_func(p->render_command_list_ptr);

        ++p;
    }
#endif
}

} // namespace hyperion::v2