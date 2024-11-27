#include <rendering/RenderResources.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

RenderResourcesBase::RenderResourcesBase()
    : m_ref_count(0),
      m_update_counter(0),
      m_is_initialized(false)
{
}

RenderResourcesBase::RenderResourcesBase(RenderResourcesBase &&other) noexcept
    : m_ref_count(other.m_ref_count.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_update_counter(other.m_update_counter.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_is_initialized(other.m_is_initialized)
{
    other.m_is_initialized = false;
}

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
                AssertThrow(!render_resources->m_is_initialized);
                
                render_resources->Initialize();

                render_resources->m_is_initialized = true;
            } else {
                HYP_FAIL("Render resources expired");
            }

            return { };
        }
    };

    if (m_ref_count.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1 == 1) {
        PUSH_RENDER_COMMAND(InitializeRenderResources, WeakRefCountedPtrFromThis());
    }
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
                AssertThrow(render_resources->m_is_initialized);

                render_resources->Destroy();

                render_resources->m_is_initialized = false;
            } else {
                HYP_FAIL("Render resources expired");
            }

            return { };
        }
    };

    const int16 ref_count = m_ref_count.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) - 1;

    if (ref_count == 0) {
        PUSH_RENDER_COMMAND(DestroyRenderResources, WeakRefCountedPtrFromThis());
    }

    AssertThrow(ref_count >= 0);
}

void RenderResourcesBase::SetNeedsUpdate()
{
    // @TODO: Change to attach this to some global manager that updates all at start of frame
    // so we dont enqueue multiple commands for one RenderResourcesBase

    // OR

    // When Set* calls happen on derived classes, don't have them lock mutex + set the member,
    // instead, have them push some command to a local queue when gets executed on Update() call.

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
                int16 current_count = render_resources->m_update_counter.Get(MemoryOrder::ACQUIRE);

                while (current_count != 0) {
                    AssertThrow(current_count > 0);

                    // Only update if already initialized.
                    if (render_resources->m_is_initialized) {
                        render_resources->Update();
                    }

                    current_count = render_resources->m_update_counter.Decrement(current_count, MemoryOrder::ACQUIRE_RELEASE) - current_count;
                }
            } else {
                HYP_FAIL("Render resources expired");
            }

            return { };
        }
    };

    m_update_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

    PUSH_RENDER_COMMAND(ApplyRenderResourcesUpdates, WeakRefCountedPtrFromThis());
}

} // namespace hyperion