/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDER_COMMAND_HPP
#define HYPERION_V2_BACKEND_RENDER_COMMAND_HPP

#include <rendering/backend/RendererResult.hpp>

#include <system/Debug.hpp>
#include <core/Containers.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/Util.hpp>
#include <Threads.hpp>
#include <Types.hpp>

#include <type_traits>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace hyperion {
namespace renderer {

using renderer::Result;

#define RENDER_COMMAND(name) RenderCommand_##name

/**
 * \brief Pushes a render command to the render command queue. This is a wrapper around RenderCommands::Push.
 */
#define PUSH_RENDER_COMMAND(name, ...) ::hyperion::renderer::RenderCommands::Push< RENDER_COMMAND(name) >(__VA_ARGS__)

/**
 * \brief Executes a render command line. This must be called from the render thread. Avoid if possible.
 */
#define EXEC_RENDER_COMMAND_INLINE(name, ...) \
    do { \
        ::hyperion::Threads::AssertOnThread(::hyperion::THREAD_RENDER); \
        RENDER_COMMAND(name)(__VA_ARGS__)(); \
    } while (0)

#define HYP_SYNC_RENDER() \
    do { \
        HYPERION_ASSERT_RESULT(::hyperion::renderer::RenderCommands::FlushOrWait()); \
    } while (0)


constexpr SizeType max_render_command_types = 128;
constexpr SizeType render_command_cache_size_bytes = 1 << 16;

using RenderCommandFunc = void(*)(void * /*ptr */, uint /* buffer_index */);

template <class T>
struct RenderCommandList
{
    struct Block
    {
        static constexpr SizeType render_command_cache_size = MathUtil::Max(render_command_cache_size_bytes / sizeof(T), 1);

        FixedArray<ValueStorage<T>, render_command_cache_size> storage;
        uint index = 0;

        HYP_FORCE_INLINE bool IsFull() const
            { return index >= uint(render_command_cache_size); }

        static_assert(render_command_cache_size >= 8, "Render command storage size is too large; runtime performance would be impacted due to needing to allocate more blocks to compensate.");
    };

    // Use a linked list so we can grow the container without invalidating pointers.
    // It's not frequent we'll be using more than just the first node, anyway.

    // Use array of 2 so we can double buffer it
    FixedArray<LinkedList<Block>, 2>    blocks;

    RenderCommandList()
    {
        blocks[0].EmplaceBack();
        blocks[1].EmplaceBack();
    }

    RenderCommandList(const RenderCommandList &other)                   = delete;
    RenderCommandList &operator=(const RenderCommandList &other)        = delete;
    RenderCommandList(RenderCommandList &&other) noexcept               = delete;
    RenderCommandList &operator=(RenderCommandList &&other) noexcept    = delete;

    ~RenderCommandList() = default;

    HYP_FORCE_INLINE
    void *AllocCommand(uint buffer_index)
    {
        // always guaranteed to have at least 1 block
        auto &blocks_buffer = blocks[buffer_index];
        Block *last_block = &blocks_buffer.Back();

        if (last_block->IsFull()) {
            blocks_buffer.EmplaceBack();
            last_block = &blocks_buffer.Back();
        }

        const SizeType command_index = last_block->index++;

        return last_block->storage.Data() + command_index;
    }

    HYP_FORCE_INLINE
    void Rewind(uint buffer_index)
    {
        auto &blocks_buffer = blocks[buffer_index];
        // Note: all items must have been destructed,
        // or undefined behavior will occur as the items are not properly destructed
        while (blocks_buffer.Size() - 1) {
            blocks_buffer.PopBack();
        }

        blocks_buffer.Front().index = 0;
    }
    
    static void RewindFunc(void *ptr, uint buffer_index)
    {
        static_cast<RenderCommandList *>(ptr)->Rewind(buffer_index);
    }
};

struct RenderCommand
{
#ifdef HYP_DEBUG_LOG_RENDER_COMMANDS
    String _debug_name;
#endif

    virtual ~RenderCommand() = default;

    HYP_FORCE_INLINE
    Result Call()
        { return operator()(); }

    virtual Result operator()() = 0;
};

struct RenderScheduler
{
    struct FlushResult
    {
        Result      result;
        SizeType    num_executed;
    };

    Array<RenderCommand *>  m_commands;

    std::mutex              m_mutex;
    AtomicVar<SizeType>     m_num_enqueued;

    RenderScheduler() = default;

    void Commit(RenderCommand *command);
    // FlushResult Flush();
    void AcceptAll(Array<RenderCommand *> &out_container);
};


struct RenderCommandHolder
{
    void                *render_command_list_ptr = nullptr;
    RenderCommandFunc   rewind_func = nullptr;
};

class RenderCommands
{
private:

    // last item must always have render_command_list_ptr be nullptr
    static FixedArray<RenderCommandHolder, max_render_command_types>    holders;
    static AtomicVar<SizeType>                                          render_command_type_index;

    static std::mutex                                                   mtx;
    static std::condition_variable                                      flushed_cv;
    static uint                                                         buffer_index;

    static RenderScheduler                                              scheduler;

public:
    template <class T, class ...Args>
    HYP_FORCE_INLINE
    static T *Push(Args &&... args)
    {
        std::unique_lock lock(mtx);
        
        void *mem = Alloc<T>(buffer_index);
        T *ptr = new (mem) T(std::forward<Args>(args)...);

#ifdef HYP_DEBUG_LOG_RENDER_COMMANDS
        ptr->_debug_name = TypeName<T>().Data();
#endif

        scheduler.Commit(ptr);

        return ptr;
    }

    HYP_FORCE_INLINE static SizeType Count()
        { return scheduler.m_num_enqueued.Get(MemoryOrder::ACQUIRE); }

    static Result Flush();
    static Result FlushOrWait();
    static void Wait();

private:
    template <class T>
    HYP_FORCE_INLINE
    static void *Alloc(uint buffer_index)
    {
        struct Data
        {
            RenderCommandFunc       rewind_func;

            RenderCommandList<T>    command_list;
            SizeType                command_type_index;

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

            Data(const Data &other)                 = delete;
            Data &operator=(const Data &other)      = delete;
            Data(Data &&other) noexcept             = delete;
            Data &operator=(Data &&other) noexcept  = delete;

            ~Data()
            {
                RenderCommands::holders[command_type_index] = RenderCommandHolder {
                    nullptr,
                    nullptr
                };
            }
        };

        static Data data;

        return data.command_list.AllocCommand(buffer_index);
    }

    static void SwapBuffers();
    static void Rewind(uint buffer_index);
};

} // namespace renderer
} // namespace hyperion

#endif