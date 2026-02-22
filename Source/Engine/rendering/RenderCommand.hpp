/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderMemory.hpp>

#include <core/debug/Debug.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/containers/LinkedList.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/StringView.hpp>

#include <core/Util.hpp>
#include <core/Types.hpp>

#include <type_traits>

namespace Hyperion {

/*! \brief Pushes a render command to the render command queue. This is a wrapper around RenderCommands::Push.
 *  If called from the render thread, the command is executed immediately. */
#define PUSH_RENDER_COMMAND(name, ...)                                                                                                           \
    if (::Hyperion::IsOnThread(::Hyperion::g_renderThread))                                                                                      \
    {                                                                                                                                            \
        const ::Hyperion::RendererResult commandResult = name(__VA_ARGS__).Call();                                                               \
        Assert(commandResult, "Render command error! [{}]: {}", commandResult.GetError().GetErrorCode(), commandResult.GetError().GetMessage()); \
    }                                                                                                                                            \
    else                                                                                                                                         \
    {                                                                                                                                            \
        ::Hyperion::RenderCommands::Push<name>(__VA_ARGS__);                                                                                     \
    }

/*! \brief If not on the render thread, waits for the render thread to finish executing all render commands. */
#define HYP_SYNC_RENDER(...)                                 \
    if (!::Hyperion::IsOnThread(::Hyperion::g_renderThread)) \
    {                                                        \
        ::Hyperion::RenderCommands::Wait();                  \
    }

constexpr uint32 maxRenderCommandTypes = 128;
constexpr SizeType renderCommandCacheSizeBytes = 1 << 16;

using RenderCommandFunc = void (*)(void* /*ptr */, uint32 /* bufferIndex */);

template <class T>
struct RenderCommandList
{
    struct Block
    {
        static constexpr SizeType renderCommandCacheSize = MathUtil::Max(renderCommandCacheSizeBytes / sizeof(T), 1);

        FixedArray<ValueStorage<T>, renderCommandCacheSize> storage;
        uint32 index = 0;

        HYP_FORCE_INLINE bool IsFull() const
        {
            return index >= uint32(renderCommandCacheSize);
        }

        static_assert(renderCommandCacheSize >= 8, "Render command storage size is too large; runtime performance would be impacted due to needing to allocate more blocks to compensate.");
    };

    // Use a linked list so we can grow the container without invalidating pointers.
    // It's not frequent we'll be using more than just the first node, anyway.

    // Use array of 2 so we can double buffer it
    FixedArray<LinkedList<Block>, 2> blocks;

    RenderCommandList()
    {
        blocks[0].EmplaceBack();
        blocks[1].EmplaceBack();
    }

    RenderCommandList(const RenderCommandList& other) = delete;
    RenderCommandList& operator=(const RenderCommandList& other) = delete;
    RenderCommandList(RenderCommandList&& other) noexcept = delete;
    RenderCommandList& operator=(RenderCommandList&& other) noexcept = delete;

    ~RenderCommandList() = default;

    HYP_FORCE_INLINE void* AllocCommand(uint32 bufferIndex)
    {
        // always guaranteed to have at least 1 block
        auto& blocksBuffer = blocks[bufferIndex];
        Block* lastBlock = &blocksBuffer.Back();

        if (lastBlock->IsFull())
        {
            blocksBuffer.EmplaceBack();
            lastBlock = &blocksBuffer.Back();
        }

        const SizeType commandIndex = lastBlock->index++;

        return lastBlock->storage.Data() + commandIndex;
    }

    HYP_FORCE_INLINE void Rewind(uint32 bufferIndex)
    {
        auto& blocksBuffer = blocks[bufferIndex];
        // Note: all items must have been destructed,
        // or undefined behavior will occur as the items are not properly destructed
        while (blocksBuffer.Size() - 1)
        {
            blocksBuffer.PopBack();
        }

        blocksBuffer.Front().index = 0;
    }

    static void RewindFunc(void* ptr, uint32 bufferIndex)
    {
        static_cast<RenderCommandList*>(ptr)->Rewind(bufferIndex);
    }
};

struct RenderCommand
{
#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
    ANSIStringView _debug_name;
#endif

    virtual ~RenderCommand() = default;

    HYP_FORCE_INLINE RendererResult Call()
    {
        return operator()();
    }

    virtual RendererResult operator()() = 0;
};

struct RenderScheduler
{
    struct FlushResult
    {
        RendererResult result;
        SizeType numExecuted;
    };

    Array<RenderCommand*> m_commands;

    std::mutex m_mutex;
    AtomicVar<SizeType> m_numEnqueued;

    RenderScheduler() = default;

    void Commit(RenderCommand* command);
    // FlushResult Flush();
    void AcceptAll(Array<RenderCommand*, RenderAllocator>& outContainer);
};

struct RenderCommandHolder
{
    void* renderCommandListPtr = nullptr;
    RenderCommandFunc rewindFunc = nullptr;
};

/*! \brief A custom, overridable render command that can be used outside of
    the main Hyperion DLL.
    \details This is useful for custom render commands that need to be executed
    in the render thread, but are not part of the main Hyperion DLL.
*/
struct HYP_API CustomRenderCommand : RenderCommand
{
    CustomRenderCommand() = default;
    virtual ~CustomRenderCommand() override = default;

    virtual RendererResult operator()() override = 0;
};

class RenderCommands
{
public:
    using RenderCommandSemaphore = threading::Semaphore<uint32>;

    struct Buffer
    {
        // last item must always have renderCommandListPtr be nullptr
        FixedArray<RenderCommandHolder, maxRenderCommandTypes> holders;
        AtomicVar<uint32> renderCommandTypeIndex;
        Mutex mtx;
        RenderScheduler scheduler;
    };

    static Buffer s_buffers[2];
    static AtomicVar<uint32> s_bufferIndex;
    static RenderCommandSemaphore s_semaphore;

public:
    HYP_FORCE_INLINE static void SwapBuffers()
    {
        s_bufferIndex.Increment(1, MemoryOrder::RELEASE);
    }

    /*! \brief Push a render command to the render command queue.
        \details The render command will be executed in the render thread.

        \tparam T The type of the render command to push. Must derive RenderCommand.
        \tparam Args The arguments to pass to the constructor of the render command.

        \param args The arguments to pass to the constructor of the render command.

        \return A pointer to the render command that was pushed.
    */
    template <class T, class... Args>
    static T* Push(Args&&... args)
    {
        static_assert(std::is_base_of_v<RenderCommand, T>, "T must derive RenderCommand");

        AssertOnThread(~g_renderThread);

        uint32 bufferIndex = CurrentBufferIndex();
        Buffer& buffer = s_buffers[bufferIndex];

        Mutex::Guard guard(buffer.mtx);

        buffer.scheduler.m_numEnqueued.Increment(1, MemoryOrder::RELEASE);

        void* mem = Alloc<T>(bufferIndex);
        T* ptr = new (mem) T(std::forward<Args>(args)...);

#ifdef HYP_RENDER_COMMANDS_DEBUG_NAME
        static const auto typeName = TypeName<T>();

        ptr->_debug_name = typeName.Data();
#endif

        buffer.scheduler.Commit(ptr);

        return ptr;
    }

    /*! \brief Push a custom render command to the render command queue.
        The class must virtually inherit from CustomRenderCommand and implement the operator() method.

        \details It is important to note that the memory for the command is NOT managed by the render command queue, but the destructor is will called without freeing the memory.
        Therefore, the object should be allocated using the following pattern:
        \code{.cpp}
        struct MyCustomRenderCommand : public CustomRenderCommand
        {
            // ... implementation here
        };

        static_cast<MyCustomRenderCommand *> command = Hyperion::Memory::AllocateAndConstruct<MyCustomRenderCommand>(...);
        Hyperion::RenderCommands::PushCustomRenderCommand(command);

        // ... elsewhere, after the command has been executed
        HYP_FREE_ALIGNED(command);
        \endcode

        \attention Ownership of the command is NOT transferred to the render command queue, so the memory must be managed elsewhere.
        HOWEVER, your calling code must NOT call the destructor of your object, as the render command queue already does this.
        This is a complexity of the design due to how render commands within the Hyperion library are typically handled, using a custom allocation method.
        However, this allocation method is not suitable for use outside of the library, so this method is provided as a workaround to still allow you to use the render command queue.
    */
    static HYP_API void PushCustomRenderCommand(CustomRenderCommand* command);

    static void Flush();
    static void Wait();

private:
    HYP_FORCE_INLINE static uint32 CurrentBufferIndex()
    {
        return s_bufferIndex.Get(MemoryOrder::ACQUIRE) % 2;
    }

    template <class T>
    HYP_FORCE_INLINE static void* Alloc(uint32 bufferIndex)
    {
        struct Data
        {
            RenderCommandList<T> commandLists[2];
            uint32 commandTypeIndices[2];

            Data()
            {
                for (uint32 i = 0; i < 2; i++)
                {
                    RenderCommands::Buffer& buffer = RenderCommands::s_buffers[i];

                    const uint32 commandTypeIndex = buffer.renderCommandTypeIndex.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

                    AssertDebug(
                        commandTypeIndex < maxRenderCommandTypes - 1,
                        "Maximum number of render command types initialized (%llu). Increase the buffer size?",
                        maxRenderCommandTypes - 1);

                    buffer.holders[commandTypeIndex] = RenderCommandHolder {
                        &commandLists[i],
                        &commandLists[i].RewindFunc
                    };

                    commandTypeIndices[i] = commandTypeIndex;
                }
            }

            Data(const Data& other) = delete;
            Data& operator=(const Data& other) = delete;
            Data(Data&& other) noexcept = delete;
            Data& operator=(Data&& other) noexcept = delete;

            ~Data()
            {
                for (uint32 i = 0; i < 2; i++)
                {
                    RenderCommands::Buffer& buffer = RenderCommands::s_buffers[i];

                    buffer.holders[commandTypeIndices[i]] = RenderCommandHolder {
                        nullptr,
                        nullptr
                    };
                }
            }
        };

        static Data data;

        return data.commandLists[bufferIndex].AllocCommand(bufferIndex);
    }

    static void Rewind(uint32 bufferIndex);
};

} // namespace Hyperion
