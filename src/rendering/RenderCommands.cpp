#include <rendering/RenderCommands.hpp>

namespace hyperion::v2 {

FixedArray<RenderCommands::HolderRef *, RenderCommands::max_render_command_types> RenderCommands::holders = { };
std::atomic<SizeType> RenderCommands::render_command_type_index = { 0 };
RenderScheduler RenderCommands::scheduler = { };

std::mutex RenderCommands::mtx = std::mutex();
std::condition_variable RenderCommands::flushed_cv = std::condition_variable();

void RenderScheduler::Commit(RenderCommand *ptr)
{
    m_commands.PushBack(ptr);
    m_num_enqueued.fetch_add(1, std::memory_order_relaxed);
}

RenderScheduler::FlushResult RenderScheduler::Flush()
{
    Threads::AssertOnThread(m_owner_thread);

    FlushResult result { Result::OK, 0 };

    SizeType index = 0;

    while (index < m_commands.Size()) {
        RenderCommand *front = m_commands[index++];

        ++result.num_executed;

#ifdef HYP_DEBUG_RENDER_COMMANDS
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

    if (HYP_LIKELY(index == m_commands.Size())) {
        m_commands.Clear();
    } else {
        AssertThrowMsg(index < m_commands.Size(), "index is > than m_commands.Size() -- incorrect bookkeeping here");

        for (const auto it = m_commands.Begin(); it != m_commands.Begin() + index;) {
            m_commands.Erase(it);
        }
    }

    m_num_enqueued.fetch_sub(index, std::memory_order_relaxed);

    return result;
}

void RenderCommands::Rewind()
{
    // all items in the cache must have had destructor called on them already.

    HolderRef **p = holders.Data();

    while (*p) {
        // const SizeType counter_value = p->counter_ptr->load();

        // if (counter_value) {
        //     Memory::Set(p->memory_ptr, 0, p->object_size * counter_value);
            
        //     p->counter_ptr->store(0);
        // }

        (*p)->Clear();

        ++p;
    }
}

} // namespace hyperion::v2