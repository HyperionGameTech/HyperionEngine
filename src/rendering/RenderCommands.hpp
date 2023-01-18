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

struct RenderCommand
{
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
    struct HolderRef
    {
        std::atomic<SizeType> *counter_ptr = nullptr;
        void *memory_ptr = nullptr;
        SizeType object_size = 0;
        SizeType object_alignment = 0;

        virtual ~HolderRef() = default;

        virtual void Clear() = 0;
    };

    static constexpr SizeType max_render_command_types = 128;
    static constexpr SizeType render_command_cache_size = 4096;

    // last item must always evaluate to false, same way null terminated char strings work
    static FixedArray<HolderRef *, max_render_command_types> holders;
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
        using RenderCommandStorageArray = Array<ValueStorage<T>, render_command_cache_size * sizeof(T)>;

        struct Data;

        struct HolderRefT : HolderRef
        {
            virtual ~HolderRefT() override = default;

            virtual void Clear() override
            {
                (static_cast<RenderCommandStorageArray *>(memory_ptr))->Clear();
            }
        };

        static struct Data
        {
            HolderRefT holder;
            RenderCommandStorageArray commands;
            std::atomic<SizeType> counter { 0 };

            Data()
            {
                // holder = { &counter, &commands, sizeof(T), alignof(T) };
                holder.memory_ptr = &commands;

                const SizeType command_type_index = RenderCommands::render_command_type_index.fetch_add(1);
                AssertThrow(command_type_index < max_render_command_types - 1);

                RenderCommands::holders[command_type_index] = &holder;
            }
        } data;

        const SizeType command_index = data.commands.Size();//data.counter.fetch_add(1);
        data.commands.PushBack({ });

        if (command_index > render_command_cache_size) {
            DebugLog(
                LogType::Warn,
                "Render command count greater than budgeted size\n"
            );
        }

        // AssertThrowMsg(cache_index < render_command_cache_size,
        //     "Render command cache size exceeded! Too many render threads are being submitted before the requests can be fulfilled.");

        return data.commands.Data() + command_index;
    }

    static void Rewind();
};

} // namespace v2
} // namespace hyperion

#endif