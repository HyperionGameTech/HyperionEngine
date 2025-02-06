#include <rendering/RenderResources.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(RenderResources, Rendering);

#pragma region RenderResourcesBase

RenderResourcesBase::RenderResourcesBase()
    : m_is_initialized(false),
      m_buffer_index(~0u),
      m_buffer_address(nullptr),
      m_ref_count(0),
      m_update_counter(0)
{
}

RenderResourcesBase::RenderResourcesBase(RenderResourcesBase &&other) noexcept
    : m_is_initialized(other.m_is_initialized),
      m_buffer_index(other.m_buffer_index),
      m_buffer_address(other.m_buffer_address),
      m_ref_count(other.m_ref_count.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_update_counter(other.m_update_counter.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
{
    other.m_is_initialized = false;
    other.m_buffer_index = ~0u;
    other.m_buffer_address = nullptr;
}

RenderResourcesBase::~RenderResourcesBase()
{
    // Ensure that the resources are no longer being used
    AssertThrowMsg(m_completion_semaphore.IsInSignalState(), "RenderResources destroyed while still in use, was WaitForCompletion() called?");
}

int RenderResourcesBase::Claim()
{
    struct RENDER_COMMAND(InitializeRenderResources) : renderer::RenderCommand
    {
        RenderResourcesBase *render_resources;

        RENDER_COMMAND(InitializeRenderResources)(RenderResourcesBase *render_resources)
            : render_resources(render_resources)
        {
        }

        virtual ~RENDER_COMMAND(InitializeRenderResources)() override = default;

        virtual RendererResult operator()() override
        {
            {
                HYP_NAMED_SCOPE("Initializing RenderResources - Initialization");

                HYP_LOG(RenderResources, Info, "Initializing RenderResources of type {}", render_resources->GetTypeName());
                    
                // Wait for pre-init to complete
                render_resources->m_pre_init_semaphore.Acquire();

                HYP_MT_CHECK_RW(render_resources->m_data_race_detector);

                AssertThrow(!render_resources->m_is_initialized);
                
                AssertThrow(render_resources->m_buffer_index == ~0u);
                render_resources->AcquireBufferIndex();

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

            return { };
        }
    };

    HYP_SCOPE;

    return m_init_semaphore.Produce(1, [this](bool is_signalled)
    {
        AssertThrow(is_signalled);

        m_completion_semaphore.Produce(1);

        PUSH_RENDER_COMMAND(InitializeRenderResources, this);
    });
}

int RenderResourcesBase::Unclaim()
{
    struct RENDER_COMMAND(DestroyRenderResources) : renderer::RenderCommand
    {
        RenderResourcesBase *render_resources;

        RENDER_COMMAND(DestroyRenderResources)(RenderResourcesBase *render_resources)
            : render_resources(render_resources)
        {
        }

        virtual ~RENDER_COMMAND(DestroyRenderResources)() override = default;

        virtual RendererResult operator()() override
        {
            HYP_NAMED_SCOPE("Destroying RenderResources");

            HYP_MT_CHECK_RW(render_resources->m_data_race_detector);

            AssertThrow(render_resources->m_is_initialized);

            if (render_resources->m_buffer_index != ~0u) {
                render_resources->ReleaseBufferIndex();
            }

            render_resources->Destroy();

            render_resources->m_is_initialized = false;

            render_resources->m_completion_semaphore.Release(1);

            return { };
        }
    };

    HYP_SCOPE;

    return m_init_semaphore.Release(1, [this](bool is_signalled)
    {
        // Must be put into non-initialized state to destroy
        AssertThrow(!is_signalled);

        PUSH_RENDER_COMMAND(DestroyRenderResources, this);
    });
}

void RenderResourcesBase::Execute(Proc<void> &&proc, bool force_render_thread)
{
    struct RENDER_COMMAND(ExecuteOnRenderThread) : renderer::RenderCommand
    {
        RenderResourcesBase *render_resources;
        Proc<void>          proc;

        RENDER_COMMAND(ExecuteOnRenderThread)(RenderResourcesBase *render_resources, Proc<void> &&proc)
            : render_resources(render_resources),
              proc(std::move(proc))
        {
        }

        virtual ~RENDER_COMMAND(ExecuteOnRenderThread)() override = default;

        virtual RendererResult operator()() override
        {
            HYP_NAMED_SCOPE("Executing RenderResources Command on Render Thread");

            HYP_MT_CHECK_RW(render_resources->m_data_race_detector);

            proc();

            render_resources->m_completion_semaphore.Release(1);

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
    PUSH_RENDER_COMMAND(ExecuteOnRenderThread, this, std::move(proc));
}

void RenderResourcesBase::SetNeedsUpdate()
{
    struct RENDER_COMMAND(ApplyRenderResourcesUpdates) : renderer::RenderCommand
    {
        RenderResourcesBase *render_resources;

        RENDER_COMMAND(ApplyRenderResourcesUpdates)(RenderResourcesBase *render_resources)
            : render_resources(render_resources)
        {
        }

        virtual ~RENDER_COMMAND(ApplyRenderResourcesUpdates)() override = default;

        virtual RendererResult operator()() override
        {
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

    PUSH_RENDER_COMMAND(ApplyRenderResourcesUpdates, this);
}

void RenderResourcesBase::WaitForCompletion()
{
    HYP_SCOPE;

    HYP_LOG(RenderResources, Debug, "Waiting for completion of RenderResources with pool index {} from thread {}", m_pool_handle.index, Threads::CurrentThreadID().name);

    if (Threads::IsOnThread(ThreadName::THREAD_RENDER)) {
        // Wait for any threads that are using this RenderResources pre-initialization to stop
        m_pre_init_semaphore.Acquire();

        // We need to execute pending tasks if we are on the render thread and
        // we still have pending tasks. Not ideal, but we need to wrap up
        // destruction of resources before we can return.
        if (!m_completion_semaphore.IsInSignalState()) {
            HYP_NAMED_SCOPE("Waiting for RenderResources tasks to complete on Render Thread");

            HYP_LOG(RenderResources, Debug, "Waiting for RenderResources tasks to complete on Render Thread");
            
            HYP_SYNC_RENDER();
        
            AssertThrow(m_completion_semaphore.IsInSignalState());
        }

        return;
    }

    // Wait for render tasks to complete
    m_completion_semaphore.Acquire();
}

void RenderResourcesBase::AcquireBufferIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertThrow(m_buffer_index == ~0u);

    GPUBufferHolderBase *holder = GetGPUBufferHolder();

    if (holder == nullptr) {
        return;
    }

    m_buffer_index = holder->AcquireIndex(&m_buffer_address);
}

void RenderResourcesBase::ReleaseBufferIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertThrow(m_buffer_index != ~0u);

    GPUBufferHolderBase *holder = GetGPUBufferHolder();
    AssertThrow(holder != nullptr);

    holder->ReleaseIndex(m_buffer_index);

    m_buffer_index = ~0u;
    m_buffer_address = nullptr;
}

#pragma endregion RenderResourcesBase

} // namespace hyperion