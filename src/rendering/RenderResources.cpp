#include <rendering/RenderResources.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(RenderResources, Rendering);

#pragma region Memory Pool

static TypeMap<UniquePtr<IRenderResourcesMemoryPool>> g_render_resources_memory_pools;
static Mutex g_render_resources_memory_pools_mutex;

HYP_API IRenderResourcesMemoryPool *GetOrCreateRenderResourcesMemoryPool(TypeID type_id, UniquePtr<IRenderResourcesMemoryPool>(*create_fn)(void))
{
    Mutex::Guard guard(g_render_resources_memory_pools_mutex);

    auto it = g_render_resources_memory_pools.Find(type_id);

    if (it == g_render_resources_memory_pools.End()) {
        it = g_render_resources_memory_pools.Set(type_id, create_fn()).first;
    }

    return it->second.Get();
}

#pragma endregion Memory Pool

#pragma region RenderResourcesBase

RenderResourcesBase::RenderResourcesBase()
    : m_is_initialized(false),
      m_buffer_index(~0u),
      m_ref_count(0),
      m_update_counter(0)
{
}

RenderResourcesBase::RenderResourcesBase(RenderResourcesBase &&other) noexcept
    : m_is_initialized(other.m_is_initialized),
      m_buffer_index(other.m_buffer_index),
      m_ref_count(other.m_ref_count.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_update_counter(other.m_update_counter.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
{
    other.m_is_initialized = false;
    other.m_buffer_index = ~0u;
}

RenderResourcesBase::~RenderResourcesBase() = default;

void RenderResourcesBase::Claim()
{
    struct RENDER_COMMAND(InitializeRenderResources) : renderer::RenderCommand
    {
        Weak<RenderResourcesBase>   render_resources_weak;

        RENDER_COMMAND(InitializeRenderResources)(const Weak<RenderResourcesBase> &render_resources_weak)
            : render_resources_weak(render_resources_weak)
        {
        }

        virtual ~RENDER_COMMAND(InitializeRenderResources)() override = default;

        virtual renderer::Result operator()() override
        {
            if (RC<RenderResourcesBase> render_resources = render_resources_weak.Lock()) {
                {
                    HYP_NAMED_SCOPE("Initializing RenderResources - Initialization");
                        
                    // Wait for pre-init to complete
                    render_resources->m_pre_init_semaphore.Acquire();

                    HYP_MT_CHECK_RW(render_resources->m_data_race_detector);

                    AssertThrow(!render_resources->m_is_initialized);
                    
                    AssertThrow(render_resources->m_buffer_index == ~0u);
                    render_resources->m_buffer_index = render_resources->AcquireBufferIndex();

                    render_resources->Initialize();
                    render_resources->m_is_initialized = true;
                }

                {
                    HYP_NAMED_SCOPE("Initializing RenderResources - Post-Initialization Update");

                    // Perform any updates that were requested before initialization
                    int16 current_count = render_resources->m_update_counter.Get(MemoryOrder::ACQUIRE);

                    while (current_count != 0) {
                        AssertThrow(current_count > 0);

                        render_resources->Update();

                        current_count = render_resources->m_update_counter.Decrement(current_count, MemoryOrder::ACQUIRE_RELEASE) - current_count;
                    }
                }
            } else {
                HYP_LOG(RenderResources, LogLevel::WARNING, "Render resources expired");
            }

            return { };
        }
    };

    HYP_SCOPE;

    m_init_semaphore.Produce(1, [this](bool is_signalled)
    {
        AssertThrow(is_signalled);

        m_completion_semaphore.Produce(1);

        PUSH_RENDER_COMMAND(InitializeRenderResources, WeakRefCountedPtrFromThis());
    });
}

void RenderResourcesBase::Unclaim()
{
    struct RENDER_COMMAND(DestroyRenderResources) : renderer::RenderCommand
    {
        Weak<RenderResourcesBase>   render_resources_weak;

        RENDER_COMMAND(DestroyRenderResources)(const Weak<RenderResourcesBase> &render_resources_weak)
            : render_resources_weak(render_resources_weak)
        {
        }

        virtual ~RENDER_COMMAND(DestroyRenderResources)() override = default;

        virtual renderer::Result operator()() override
        {
            if (RC<RenderResourcesBase> render_resources = render_resources_weak.Lock()) {
                HYP_NAMED_SCOPE("Destroying RenderResources");

                HYP_MT_CHECK_RW(render_resources->m_data_race_detector);

                AssertThrow(render_resources->m_is_initialized);

                if (render_resources->m_buffer_index != ~0u) {
                    render_resources->ReleaseBufferIndex(render_resources->m_buffer_index);
                    render_resources->m_buffer_index = ~0u;
                }

                render_resources->Destroy();

                render_resources->m_is_initialized = false;

                render_resources->m_completion_semaphore.Release(1);
            } else {
                HYP_LOG(RenderResources, LogLevel::WARNING, "Render resources expired");

                HYP_FAIL("Render resources expired before safe destruction could be performed");
            }

            return { };
        }
    };

    HYP_SCOPE;

    m_init_semaphore.Release(1, [this](bool is_signalled)
    {
        // Must be put into non-initialized state to destroy
        AssertThrow(!is_signalled);

        PUSH_RENDER_COMMAND(DestroyRenderResources, WeakRefCountedPtrFromThis());
    });
}

void RenderResourcesBase::Execute(Proc<void> &&proc, bool force_render_thread)
{
    struct RENDER_COMMAND(ExecuteOnRenderThread) : renderer::RenderCommand
    {
        Weak<RenderResourcesBase>   render_resources_weak;
        Proc<void>                  proc;

        RENDER_COMMAND(ExecuteOnRenderThread)(const Weak<RenderResourcesBase> &render_resources_weak, Proc<void> &&proc)
            : render_resources_weak(render_resources_weak),
              proc(std::move(proc))
        {
        }

        virtual ~RENDER_COMMAND(ExecuteOnRenderThread)() override = default;

        virtual renderer::Result operator()() override
        {
            if (RC<RenderResourcesBase> render_resources = render_resources_weak.Lock()) {
                HYP_NAMED_SCOPE("Executing RenderResources Command on Render Thread");

                HYP_MT_CHECK_RW(render_resources->m_data_race_detector);

                proc();

                render_resources->m_completion_semaphore.Release(1);
            } else {
                HYP_LOG(RenderResources, LogLevel::WARNING, "Render resources expired");
            }

            return { };
        }
    };

    HYP_SCOPE;

    m_completion_semaphore.Produce(1);

    if (!force_render_thread) {
        if (!IsInitialized()) {
            // Initialization (happens on render thread) will wait for this value to hit zero
            m_pre_init_semaphore.Produce(1);

            HYP_DEFER({
                m_pre_init_semaphore.Release(1);
            });

            // Check again, as it might have been initialized in between the initial check and the increment
            if (!IsInitialized()) {
                HYP_NAMED_SCOPE("Executing RenderResources Command - Inline");

                { // Critical section
                    HYP_MT_CHECK_RW(m_data_race_detector);

                    // Execute inline if not initialized yet instead of pushing to render thread
                    proc();

                    m_completion_semaphore.Release(1);
                }

                return;
            }
        }
    }

    // Execute on render thread
    PUSH_RENDER_COMMAND(ExecuteOnRenderThread, WeakRefCountedPtrFromThis(), std::move(proc));
}

void RenderResourcesBase::SetNeedsUpdate()
{
    struct RENDER_COMMAND(ApplyRenderResourcesUpdates) : renderer::RenderCommand
    {
        Weak<RenderResourcesBase>   render_resources_weak;

        RENDER_COMMAND(ApplyRenderResourcesUpdates)(const Weak<RenderResourcesBase> &render_resources_weak)
            : render_resources_weak(render_resources_weak)
        {
        }

        virtual ~RENDER_COMMAND(ApplyRenderResourcesUpdates)() override = default;

        virtual renderer::Result operator()() override
        {
            if (RC<RenderResourcesBase> render_resources = render_resources_weak.Lock()) {
                HYP_NAMED_SCOPE("Applying RenderResources Updates on Render Thread");
                
                int16 current_count = render_resources->m_update_counter.Get(MemoryOrder::ACQUIRE);

                while (current_count != 0) {
                    HYP_MT_CHECK_RW(render_resources->m_data_race_detector);

                    AssertThrow(current_count > 0);

                    // Only should get here if already initialized due to the check when SetNeedsUpdate() is called
                    AssertThrow(render_resources->m_is_initialized);
                    
                    render_resources->Update();

                    current_count = render_resources->m_update_counter.Decrement(current_count, MemoryOrder::ACQUIRE_RELEASE) - current_count;
                }

                render_resources->m_completion_semaphore.Release(1);
            } else {
                HYP_LOG(RenderResources, LogLevel::WARNING, "Render resources expired");
            }

            return { };
        }
    };

    HYP_SCOPE;

    m_completion_semaphore.Produce(1);

    // If not yet initialized, increment the counter and return immediately
    // Otherwise, we need to push a command to the render thread to update the resources
    if (!IsInitialized()) {
        m_pre_init_semaphore.Produce(1);

        HYP_DEFER({
            m_pre_init_semaphore.Release(1);
        });

        // Check again, as it might have been initialized in between the initial check and the increment
        if (!IsInitialized()) {
            m_update_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

            m_completion_semaphore.Release(1);

            return;
        }
    }

    m_update_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

    PUSH_RENDER_COMMAND(ApplyRenderResourcesUpdates, WeakRefCountedPtrFromThis());
}

void RenderResourcesBase::WaitForCompletion()
{
    HYP_SCOPE;

    HYP_LOG(RenderResources, LogLevel::DEBUG, "Waiting for completion of RenderResources with pool index {} from thread {}", m_pool_handle.index, Threads::CurrentThreadID().name);

    if (Threads::IsOnThread(ThreadName::THREAD_RENDER)) {
        // Wait for any threads that are using this RenderResources pre-initialization to stop
        m_pre_init_semaphore.Acquire();

        // We need to execute pending tasks if we are on the render thread and
        // we still have pending tasks. Not ideal, but we need to wrap up
        // destruction of resources before we can return.
        if (!m_completion_semaphore.IsInSignalState()) {
            HYP_NAMED_SCOPE("Waiting for RenderResources tasks to complete on Render Thread");

            HYP_LOG(RenderResources, LogLevel::DEBUG, "Waiting for RenderResources tasks to complete on Render Thread");
            
            HYP_SYNC_RENDER();
        
            AssertThrow(m_completion_semaphore.IsInSignalState());
        }

        return;
    }

    // Wait for render tasks to complete
    m_completion_semaphore.Acquire();
}

#pragma endregion RenderResourcesBase

} // namespace hyperion