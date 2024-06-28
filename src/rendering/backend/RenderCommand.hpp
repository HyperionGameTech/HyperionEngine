/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDER_COMMAND_HPP
#define HYPERION_BACKEND_RENDER_COMMAND_HPP

#include <rendering/backend/RendererResult.hpp>

#include <core/system/Debug.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Threads.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/utilities/StringView.hpp>
#include <core/Util.hpp>

#include <Types.hpp>

#include <type_traits>
#include <mutex>
#include <condition_variable>

namespace hyperion {
namespace renderer {

using renderer::Result;

#define RENDER_COMMAND(name) RenderCommand_##name

/*! \brief Pushes a render command to the render command queue. This is a wrapper around RenderCommands::Push.
 *  If called from the render thread, the command is executed immediately. */
#define PUSH_RENDER_COMMAND(name, ...) \
    if (::hyperion::Threads::IsOnThread(::hyperion::ThreadName::THREAD_RENDER)) { \
        const ::hyperion::renderer::Result command_result = RENDER_COMMAND(name)(__VA_ARGS__).Call(); \
        AssertThrowMsg(command_result, "Render command error! [%d]: %s\n", command_result.error_code, command_result.message); \
    } else { \
        ::hyperion::renderer::RenderCommands::Push< RENDER_COMMAND(name) >(__VA_ARGS__); \
    }

/*! \brief If not on the render thread, waits for the render thread to finish executing all render commands. */
#define HYP_SYNC_RENDER(...) \
    if (!::hyperion::Threads::IsOnThread(::hyperion::ThreadName::THREAD_RENDER)) { \
        ::hyperion::renderer::RenderCommands::Wait(); \
    }


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
#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
    ANSIStringView  _debug_name;
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

/*! \brief A custom, overridable render command that can be used outside of
    the main Hyperion DLL.
    \details This is useful for custom render commands that need to be executed
    in the render thread, but are not part of the main Hyperion DLL. 
*/
struct HYP_API RENDER_COMMAND(CustomRenderCommand) : RenderCommand
{
    RENDER_COMMAND(CustomRenderCommand)()                   = default;
    virtual ~RENDER_COMMAND(CustomRenderCommand)() override = default;

    virtual Result operator()() override = 0;
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
    /*! \brief Push a render command to the render command queue.
        \details The render command will be executed in the render thread.

        \tparam T The type of the render command to push. Must derive RenderCommand.
        \tparam Args The arguments to pass to the constructor of the render command.

        \param args The arguments to pass to the constructor of the render command.

        \return A pointer to the render command that was pushed.
    */
    template <class T, class ...Args>
    HYP_FORCE_INLINE
    static T *Push(Args &&... args)
    {
        static_assert(std::is_base_of_v<RenderCommand, T>, "T must derive RenderCommand");

        Threads::AssertOnThread(~ThreadName::THREAD_RENDER);

        std::unique_lock lock(mtx);
        
        void *mem = Alloc<T>(buffer_index);
        T *ptr = new (mem) T(std::forward<Args>(args)...);

#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
        static const auto type_name = TypeName<T>();
        
        ptr->_debug_name = type_name.Data();
#endif

        scheduler.Commit(ptr);

        return ptr;
    }

    /*! \brief Push a custom render command to the render command queue.
        The class must virtually inherit from RENDER_COMMAND(CustomRenderCommand) and implement the operator() method.

        \details It is important to note that the memory for the command is NOT managed by the render command queue, but the destructor is will called without freeing the memory.
        Therefore, the object should be allocated using the following pattern:
        \code{.cpp}
        struct MyCustomRenderCommand : public RENDER_COMMAND(CustomRenderCommand)
        {
            // ... implementation here
        };

        static_cast<MyCustomRenderCommand *> command = hyperion::Memory::AllocateAndConstruct<MyCustomRenderCommand>(...);
        hyperion::renderer::RenderCommands::PushCustomRenderCommand(command);

        // ... elsewhere, after the command has been executed
        hyperion::Memory::Free(command);
        \endcode

        \attention Ownership of the command is NOT transferred to the render command queue, so the memory must be managed elsewhere.
        HOWEVER, your calling code must NOT call the destructor of your object, as the render command queue already does this.
        This is a complexity of the design due to how render commands within the Hyperion library are typically handled, using a custom allocation method.
        However, this allocation method is not suitable for use outside of the library, so this method is provided as a workaround to still allow you to use the render command queue.
    */
    static HYP_API void PushCustomRenderCommand(RENDER_COMMAND(CustomRenderCommand) *command);

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
                command_type_index = RenderCommands::render_command_type_index.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

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