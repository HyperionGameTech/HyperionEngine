#ifndef HYPERION_V2_RENDER_COMMANDS_HPP
#define HYPERION_V2_RENDER_COMMANDS_HPP

#include <rendering/backend/RendererResult.hpp>

#include <system/Debug.hpp>
#include <core/Containers.hpp>
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
constexpr SizeType render_command_cache_size = 4096;

template <class T>
using RenderCommandStorageArray = Array<ValueStorage<T>, render_command_cache_size * sizeof(T)>;


struct RenderCommandHolder
{
    void *memory_ptr = nullptr;

    virtual ~RenderCommandHolder() = default;

    virtual void Clear() = 0;
};

template <class T>
struct RenderCommandHolderDerived : RenderCommandHolder
{
    virtual ~RenderCommandHolderDerived() override = default;

    virtual void Clear() override
    {
        (static_cast<RenderCommandStorageArray<T> *>(memory_ptr))->Clear();
    }
};

#if 0
template <class T>
struct RenderCommandList
{
    struct RenderCommandStorage
    {
        FixedArray<ValueStorage<T>, render_command_cache_size> items;
        std::atomic<UInt> index { 0 };
    };


    struct RenderCommandHolderDerived : RenderCommandHolder
    {
        virtual ~RenderCommandHolderDerived() override = default;

        virtual void Clear() override
        {
            (static_cast<RenderCommandLinkedList *>(memory_ptr))->Clear();
        }
    };


    struct Node
    {
        std::atomic<Node *> next;
        RenderCommandStorage storage;

        Node()
            : next { nullptr }
        {
        }

        ~Node()
        {
            if (Node *_next = next.load()) {
                delete _next;
            }
        }

        bool IsFull() const
            { return storage.index.load(std::memory_order_acquire) >= render_command_cache_size; }
    };

    Node *head;
    RenderCommandHolderDerived holder;

    RenderCommandList()
        : head(new Node),
          tail { head }
    {

        holder.memory_ptr = this;

        const SizeType command_type_index = RenderCommands::render_command_type_index.fetch_add(1);
        AssertThrow(command_type_index < max_render_command_types - 1);

        RenderCommands::holders[command_type_index] = &holder;
    }

    ~RenderCommandList()
    {
        Rewind();
        delete head;
    }

    Node *Head()
        { return head; }

    const Node *Head() const
        { return head; }

    Node *Tail()
    {
        UInt index_counter = 0;

        Node *_head = head;
        Node *_next = nullptr;

        do {
            Node *next_before = _next;
            _next = _head->next.load();
            _head = next_before;
        } while (_next != nullptr);

        return _head;
    }

    void EmplaceRenderCommand()
    {
        Node *tail = Tail();
    }

    Node *Insert()
    {
        Node *_head = head;
        Node *new_node = new Node;

        while (!_head.next.compare_exchange_strong(nullptr, new_node));

        return new_node;
    }

    Node *AtIndex(UInt index)
    {
        UInt index_counter = 0;

        Node *_head = head;

        for (UInt i = 0; i <= index; i++) {
            _head = _head->next.load();

            if (_head == nullptr) {
                return nullptr;
            }
        }

        return _head;
    }

    void Rewind()
    {
        Node *_head = head;

        while (_head != nullptr) {
            Node *previous_head = _head;
            _head = _head->next.exchange(nullptr);

            if (previous_head != head) {
                delete previous_head;
            }

            // all items must have had destructor called!
            previous_head->storage.index.store(0, std::memory_order_release);
        }
    }
};

#endif

template <class T>
struct RenderCommandList
{
    using Block = FixedArray<ValueStorage<T>, render_command_cache_size>;

    LinkedList<Block> blocks;

    RenderCommandList()
    {
        blocks.PushBack({ });
    }

    void Push()
    {

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
    std::atomic<SizeType> m_num_enqueued;
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

class RenderCommands
{
private:

    // last item must always evaluate to false, same way null terminated char strings work
    static FixedArray<RenderCommandHolder *, max_render_command_types> holders;
    static std::atomic<SizeType> render_command_type_index;

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
    {
        scheduler.SetOwnerThreadID(id);
    }

    HYP_FORCE_INLINE static SizeType Count()
    {
        return scheduler.m_num_enqueued.load();
    }

    HYP_FORCE_INLINE static Result Flush()
    {
        if (Count() == 0) {
            HYPERION_RETURN_OK;
        }

        std::unique_lock lock(mtx);

        auto flush_result = scheduler.Flush();
        if (flush_result.num_executed) {
            Rewind();

            scheduler.m_num_enqueued.fetch_sub(flush_result.num_executed);
        }

        lock.unlock();
        flushed_cv.notify_all();

        return flush_result.result;
    }

    HYP_FORCE_INLINE static Result FlushOrWait()
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

    HYP_FORCE_INLINE static void Wait()
    {
        std::unique_lock lock(mtx);
        flushed_cv.wait(lock, [] { return RenderCommands::Count() == 0; });
    }

private:
    template <class T>
    static void *Alloc()
    {
        struct Data
        {
            RenderCommandHolderDerived<T> holder;
            RenderCommandStorageArray<T> commands;

            Data()
            {
                holder.memory_ptr = &commands;

                const SizeType command_type_index = RenderCommands::render_command_type_index.fetch_add(1);
                AssertThrow(command_type_index < max_render_command_types - 1);

                RenderCommands::holders[command_type_index] = &holder;
            }
        };

        static Data data;

        const SizeType command_index = data.commands.Size();
        data.commands.PushBack({ });

        if (command_index >= render_command_cache_size) {
            DebugLog(
                LogType::Warn,
                "Render command count greater than budgeted size! (size: %llu/%llu, in function: %s)\n",
                data.commands.Size(),
                render_command_cache_size,
                HYP_DEBUG_FUNC
            );

            // HYP_BREAKPOINT;
        }

        return data.commands.Data() + command_index;
    }

    static void Rewind();
};

} // namespace v2
} // namespace hyperion

#endif