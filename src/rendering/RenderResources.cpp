#include <rendering/RenderResources.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

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
                HYP_FAIL("Render resources expired");
            }

            return { };
        }
    };

    HYP_SCOPE;

    m_init_semaphore.Produce(1, [this](bool is_signalled)
    {
        AssertThrow(is_signalled);

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
                AssertThrow(render_resources->m_buffer_index != ~0u);

                render_resources->ReleaseBufferIndex(render_resources->m_buffer_index);
                render_resources->m_buffer_index = ~0u;

                render_resources->Destroy();

                render_resources->m_is_initialized = false;
            } else {
                HYP_FAIL("Render resources expired");
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
            } else {
                HYP_FAIL("Render resources expired");
            }

            return { };
        }
    };

    HYP_SCOPE;

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
            } else {
                HYP_FAIL("Render resources expired");
            }

            return { };
        }
    };

    HYP_SCOPE;

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

            return;
        }
    }

    m_update_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

    PUSH_RENDER_COMMAND(ApplyRenderResourcesUpdates, WeakRefCountedPtrFromThis());
}

} // namespace hyperion