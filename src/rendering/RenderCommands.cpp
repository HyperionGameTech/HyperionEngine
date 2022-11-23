#include <rendering/RenderCommands.hpp>

namespace hyperion::v2 {

HeapArray<RenderCommands::HolderRef, RenderCommands::max_render_command_types> RenderCommands::holders = { };
std::atomic<SizeType> RenderCommands::render_command_type_index = { 0 };
RenderScheduler RenderCommands::scheduler = { };

RenderScheduler::FlushResult RenderScheduler::Flush(Engine *engine)
{
    FlushResult result { Result::OK, 0 };

    std::unique_lock lock(m_mutex);
    SizeType count = m_commands.Size();

    while (count) {
        --count;

        RenderCommandBase2 *front = m_commands.Front();

        ++result.num_executed;

        if (!(result.result = front->Call(engine))) {
            front->~RenderCommandBase2();

            SizeType num_executed = result.num_executed;
            m_num_enqueued.fetch_sub(num_executed, std::memory_order_relaxed);

            while (num_executed) {
                m_commands.PopFront();
                --num_executed;
            }

            return result;
        }

        front->~RenderCommandBase2();
    }

    m_commands.Clear();

    m_num_enqueued.store(0, std::memory_order_relaxed);

    lock.unlock();
    m_flushed_cv.notify_all();

    return result;
}

RenderScheduler::FlushResult RenderScheduler::FlushOrWait(Engine *engine)
{
    if (Threads::IsOnThread(m_owner_thread)) {
        return Flush(engine);
    }

    Wait();

    return { Result::OK, 0 };
}

void RenderScheduler::Wait()
{
    std::unique_lock lock(m_mutex);
    m_flushed_cv.wait(lock, [this] { return m_num_enqueued.load(std::memory_order_relaxed) == 0; });
}

} // namespace hyperion::v2