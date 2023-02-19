#ifndef HYPERION_V2_RENDER_COMMANDS_HPP
#define HYPERION_V2_RENDER_COMMANDS_HPP

#include <rendering/backend/RendererResult.hpp>

#include <system/Debug.hpp>
#include <core/Containers.hpp>
#include <core/lib/AtomicVar.hpp>
#include <Threads.hpp>
#include <Types.hpp>

#include <type_traits>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace hyperion {
namespace v2 {

class Engine;

using renderer::Result;

#define RENDER_COMMAND(name) RenderCommand_##name
#define PUSH_RENDER_COMMAND(name, ...) RenderCommands::Push< RENDER_COMMAND(name) >(__VA_ARGS__)

#define EXEC_RENDER_COMMAND_INLINE(name, ...) \
    { \
        Threads::AssertOnThread(THREAD_RENDER); \
        RENDER_COMMAND(name) _command(__VA_ARGS__)(); \
    }

constexpr SizeType max_render_command_types = 128;
constexpr SizeType render_command_cache_size = 128;


using RenderCommandRewindFunc = void(*)(void *);

template <class T>
struct RenderCommandList
{
    struct Block
    {
        FixedArray<ValueStorage<T>, render_command_cache_size> storage;
        UInt index = 0;

        HYP_FORCE_INLINE
        bool IsFull() const
            { return index >= render_command_cache_size; }
    };

    // Use a linked list so we can grow the container without invalidating pointers.
    // It's not frequent we'll be using more than just the first node, anyway.
    LinkedList<Block> blocks;

    RenderCommandList()
    {
        blocks.EmplaceBack();
    }

    RenderCommandList(const RenderCommandList &other) = delete;
    RenderCommandList &operator=(const RenderCommandList &other) = delete;

    RenderCommandList(RenderCommandList &&other) noexcept = delete;
    RenderCommandList &operator=(RenderCommandList &&other) noexcept = delete;

    ~RenderCommandList() = default;

    HYP_FORCE_INLINE
    void *AllocCommand()
    {
        // always guaranteed to have at least 1 block
        Block *last_block = &blocks.Back();

        if (last_block->IsFull()) {
            DebugLog(
                LogType::Debug,
                "Allocating new block node for render commands.\n"
            );

            blocks.EmplaceBack();
            last_block = &blocks.Back();
        }

        const SizeType command_index = last_block->index++;

        return last_block->storage.Data() + command_index;
    }

    void Rewind()
    {
        // Note: all items must have been destructed,
        // or undefined behavior will occur as the items are not properly destructed
        while (blocks.Size() != 1) {
            blocks.PopBack();
        }

        blocks.Front().index = 0;
    }

    static void RewindFunc(void *ptr)
    {
        static_cast<RenderCommandList *>(ptr)->Rewind();
    }
};

struct RenderCommand
{
#ifdef HYP_DEBUG_LOG_RENDER_COMMANDS
    const char *_debug_name = "";
#endif

    virtual ~RenderCommand() = default;

    HYP_FORCE_INLINE Result Call()
        { return operator()(); }

    virtual Result operator()() = 0;
};

struct RenderScheduler
{
    struct FlushResult
    {
        Result result;
        SizeType num_executed;
    };

    Array<RenderCommand *> m_commands;

    std::mutex m_mutex;
    AtomicVar<SizeType> m_num_enqueued;
    std::condition_variable m_flushed_cv;
    ThreadID m_owner_thread;

    RenderScheduler()
        : m_owner_thread(ThreadID::invalid)
    {
    }

    ThreadID GetOwnerThreadID() const
        { return m_owner_thread; }

    void SetOwnerThreadID(ThreadID id)
        { m_owner_thread = id; }

    void Commit(RenderCommand *command);

    FlushResult Flush();
};


struct RenderCommandHolder
{
    void *render_command_list_ptr = nullptr;
    RenderCommandRewindFunc rewind_func = nullptr;
};

class RenderCommands
{
private:

    // last item must always have render_command_list_ptr be nullptr
    static FixedArray<RenderCommandHolder, max_render_command_types> holders;
    static AtomicVar<SizeType> render_command_type_index;

    static std::mutex mtx;
    static std::condition_variable flushed_cv;

    static RenderScheduler scheduler;

public:
    template <class T, class ...Args>
    static T *Push(Args &&... args)
    {
        std::unique_lock lock(mtx);
        
        void *mem = Alloc<T>();
        T *ptr = new (mem) T(std::forward<Args>(args)...);

#ifdef HYP_DEBUG_LOG_RENDER_COMMANDS
        ptr->_debug_name = typeid(T).name();
#endif

        scheduler.Commit(ptr);

        return ptr;
    }

    static void SetOwnerThreadID(ThreadID id)
        { scheduler.SetOwnerThreadID(id); }

    HYP_FORCE_INLINE static SizeType Count()
        { return scheduler.m_num_enqueued.Get(MemoryOrder::ACQUIRE); }

    static Result Flush();
    static Result FlushOrWait();
    static void Wait();

private:
    template <class T>
    static void *Alloc()
    {
        struct Data
        {
            RenderCommandRewindFunc rewind_func;
            RenderCommandList<T> command_list;
            SizeType command_type_index;

            Data()
            {
                command_type_index = RenderCommands::render_command_type_index.Increment(1, MemoryOrder::SEQUENTIAL);

                AssertThrowMsg(
                    command_type_index < max_render_command_types - 1,
                    "Maximum number of render command types initialized (%llu). Increase the buffer size?",
                    max_render_command_types - 1
                );

                rewind_func = &command_list.RewindFunc;

                RenderCommands::holders[command_type_index] = RenderCommandHolder {
                    &command_list,
                    rewind_func
                };
            }

            Data(const Data &other) = delete;
            Data &operator=(const Data &other) = delete;

            Data(Data &&other) noexcept = delete;
            Data &operator=(Data &&other) noexcept = delete;

            ~Data()
            {
                RenderCommands::holders[command_type_index] = RenderCommandHolder {
                    nullptr,
                    nullptr
                };
            }
        };

        static Data data;

        return data.command_list.AllocCommand();
    }

    static void Rewind();
};

} // namespace v2
} // namespace hyperion

#endif