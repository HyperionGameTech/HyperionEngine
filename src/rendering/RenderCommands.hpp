#ifndef HYPERION_V2_RENDER_COMMANDS_HPP
#define HYPERION_V2_RENDER_COMMANDS_HPP

#include <rendering/backend/RendererResult.hpp>

#include <system/Debug.hpp>
#include <core/Containers.hpp>
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

template <class T>
struct RenderCommandData2 { };

struct RenderCommand2_UpdateEntityData;

template <>
struct RenderCommandData2<RenderCommand2_UpdateEntityData>
{
    Int x[64];
};

struct RenderCommandBase2
{
    Result(*fn_ptr)(RenderCommandBase2 *, Engine *);

    virtual ~RenderCommandBase2() = default;

    HYP_FORCE_INLINE Result Call(Engine *engine)
    {
        return operator()(engine);
    }

    virtual Result operator()(Engine *engine) = 0;
};

struct RenderCommand2_UpdateEntityData : RenderCommandBase2
{
    RenderCommandData2<RenderCommand2_UpdateEntityData> data;

    RenderCommand2_UpdateEntityData()
    {
        RenderCommandBase2::fn_ptr = [](RenderCommandBase2 *self, Engine *engine) -> Result
        {
            auto *self_casted = static_cast<RenderCommand2_UpdateEntityData *>(self);
            self_casted->data.x[0] = 123;

            volatile int y = 0;

            for (int x = 0; x < 100; x++) {
                ++y;
            }

            HYPERION_RETURN_OK;
        };
    }

    virtual ~RenderCommand2_UpdateEntityData() = default;

    // Int x[64];

    // virtual Result operator()(Engine *engine)
    // {
    //     // data.shader_data.bucket = Bucket::BUCKET_INTERNAL;

    //     x[0] = 123;

    //     volatile int y = 0;

    //     for (int x = 0; x < 100; x++) {
    //         ++y;
    //     }

    //     HYPERION_RETURN_OK;
    // }
};

#define DEFINE_RENDER_COMMAND(name, fnptr) \
    struct RenderCommand_##name : RenderCommandBase2 \
    { \
        RenderCommandData2< RenderCommand_##name > data; \
        RenderCommand_##name() \
        { \
            RenderCommandBase2::fn_ptr = &Execute; \
        } \
        \
        RenderCommand_##name(RenderCommandData2< RenderCommand_##name > &&data) \
            : data(std::move(data)) \
        { \
            RenderCommandBase2::fn_ptr = &Execute; \
        } \
        \
        virtual ~RenderCommand_##name() = default; \
        \
        \
        static Result Execute(RenderCommandBase2 *ptr, Engine *engine) \
        { \
            auto tmp = (fnptr); \
            return tmp(static_cast< RenderCommand_##name * >(ptr), engine); \
        } \
    };


// struct RENDER_COMMAND(FooBar);

// template <>
// struct RenderCommandData2<RenderCommand_FooBar>
// {
//     Int x[64];
// };

// DEFINE_RENDER_COMMAND(FooBar, [](RenderCommand_FooBar *self, Engine *engine) -> Result {
//     self->data.x[0] = 123;

//     volatile int y = 0;

//     for (int x = 0; x < 100; x++) {
//         ++y;
//     }

//     HYPERION_RETURN_OK;
// });

struct RENDER_COMMAND(FooBar) : RenderCommandBase2
{
    Int x[64];

    virtual Result operator()(Engine *engine) final
    {
        x[0] = 123;

        volatile int y = 0;

        for (int x = 0; x < 100; x++) {
            ++y;
        }

        HYPERION_RETURN_OK;
    }
};


struct RenderScheduler
{
    struct FlushResult
    {
        Result result;
        SizeType num_executed;
    };

    Array<RenderCommandBase2 *> m_commands;

    std::mutex m_mutex;
    std::atomic<SizeType> m_num_enqueued;
    std::condition_variable m_flushed_cv;
    ThreadID m_owner_thread;

    RenderScheduler()
        : m_owner_thread(ThreadID::invalid)
    {
    }

    void SetOwnerThreadID(ThreadID id)
    {
        m_owner_thread = id;
    }

    void Commit(RenderCommandBase2 *command);

    FlushResult Flush(Engine *engine);
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

        explicit operator bool() const
        {
            return counter_ptr != nullptr;
        }
    };

    static constexpr SizeType max_render_command_types = 128;
    static constexpr SizeType render_command_cache_size = 1024;

    // last item must always evaluate to false, same way null terminated char strings work
    static HeapArray<HolderRef, max_render_command_types> holders;
    static std::atomic<SizeType> render_command_type_index;

    static std::mutex mtx;
    static std::condition_variable flushed_cv;

    static RenderScheduler scheduler;

public:
    template <class T, class ...Args>
    static T *Push(Args &&... args)
    {
        std::lock_guard lock(mtx);
        
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
        return scheduler.m_num_enqueued.load(std::memory_order_relaxed);
    }

    HYP_FORCE_INLINE static Result Flush(Engine *engine)
    {
        // if (Count() == 0) {
        //     HYPERION_RETURN_OK;
        // }

        std::unique_lock lock(mtx);

        auto flush_result = scheduler.Flush(engine);
        if (flush_result.num_executed) {
            Rewind();
        }

        lock.unlock();
        flushed_cv.notify_all();

        return flush_result.result;
    }

    HYP_FORCE_INLINE static Result FlushOrWait(Engine *engine)
    {
        if (Count() == 0) {
            HYPERION_RETURN_OK;
        }

        if (Threads::CurrentThreadID() == scheduler.m_owner_thread) {
            return Flush(engine);
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
        static struct Data
        {
            alignas(T) std::byte cache[sizeof(T) * render_command_cache_size] = { };
            std::atomic<SizeType> counter { 0 };

            Data()
            {
                // zero out cache memory
                Memory::Set(cache, 0x00, sizeof(T) * render_command_cache_size);

                SizeType index = RenderCommands::render_command_type_index.fetch_add(1);
                AssertThrow(index < max_render_command_types - 1);

                RenderCommands::holders[index] = HolderRef { &counter, (void *)cache, sizeof(T), alignof(T) };
            }
        } data;

        const SizeType cache_index = data.counter.fetch_add(1);

        AssertThrowMsg(cache_index < render_command_cache_size,
            "Render command cache size exceeded! Too many render threads are being submitted before the requests can be fulfilled.");

        return data.cache + (cache_index * sizeof(T));
    }

    static void Rewind();
};

} // namespace v2
} // namespace hyperion

#endif