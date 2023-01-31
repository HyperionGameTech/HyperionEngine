#include <rendering/RenderCommands.hpp>

namespace hyperion::v2 {

FixedArray<RenderCommands::RenderCommandHolder *, RenderCommands::max_render_command_types> RenderCommands::holders = { };
std::atomic<SizeType> RenderCommands::render_command_type_index = { 0 };
RenderScheduler RenderCommands::scheduler = { };

std::mutex RenderCommands::mtx = std::mutex();
std::condition_variable RenderCommands::flushed_cv = std::condition_variable();

void RenderScheduler::Commit(RenderCommand *command)
{
    m_commands.PushBack(command);
    m_num_enqueued.fetch_add(1, std::memory_order_relaxed);
}

RenderScheduler::FlushResult RenderScheduler::Flush()
{
    Threads::AssertOnThread(m_owner_thread);

    FlushResult result { Result::OK, 0 };

    SizeType index = 0;
    const SizeType num_commands = m_commands.Size();

    while (index < num_commands) {
        RenderCommand *front = m_commands[index++];

        ++result.num_executed;

#ifdef HYP_DEBUG_LOG_RENDER_COMMANDS
        DebugLog(LogType::RenDebug, "Executing render command %s\n", typeid(*front).name());
#endif

        result.result = (*front)();
        front->~RenderCommand();

        // check if an error occurred
        if (!result.result) {
            DebugLog(LogType::Error, "Render command error! %s\n", result.result.message);

            break;
        }
    }

    if (HYP_LIKELY(index == num_commands)) {
        m_commands.Clear();
    } else {
        AssertThrowMsg(index < num_commands, "index is > than m_commands.Size() -- incorrect bookkeeping here");

        const SizeType erase_to_index = index;

        DebugLog(LogType::Debug, "Erasing render commands to index: %llu\n", erase_to_index);

        for (SizeType i = 0; i < erase_to_index; i++) {
            m_commands.PopFront();
        }

        m_commands.Refit();
    }

    m_num_enqueued.fetch_sub(index, std::memory_order_relaxed);

    return result;
}

void RenderCommands::Rewind()
{
    // all items in the cache must have had destructor called on them already.

    RenderCommandHolder **pp = holders.Data();

    while (*pp) {
        (*pp)->Clear();

        ++pp;
    }
}

} // namespace hyperion::v2