#include <rendering/RenderResources.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

RenderResourcesBase::RenderResourcesBase()
    : m_ref_count(0),
      m_update_counter(0)
{
}

RenderResourcesBase::RenderResourcesBase(RenderResourcesBase &&other) noexcept
    : m_ref_count(other.m_ref_count),
      m_update_counter(other.m_update_counter.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
{
    other.m_ref_count = 0;
}

void RenderResourcesBase::Claim()
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
#endif

    if (m_ref_count++ == 0) {
        Initialize();
    }
}

void RenderResourcesBase::Unclaim()
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
#endif

    AssertThrow(m_ref_count != 0);

    if (--m_ref_count == 0) {
        Destroy();
    }
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
                while (render_resources->m_update_counter.Get(MemoryOrder::ACQUIRE) != 0) {
                    // HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Updating render resources {}", (void *)this);

                    // Only update if m_ref_count is not zero (data would not be initialized otherwise)
                    if (render_resources->m_ref_count != 0) {
                        render_resources->Update();
                    }

                    render_resources->m_update_counter.Set(0, MemoryOrder::RELEASE);
                }
            }

            return { };
        }
    };

    m_update_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

    PUSH_RENDER_COMMAND(ApplyRenderResourcesUpdates, WeakRefCountedPtrFromThis());
}

} // namespace hyperion