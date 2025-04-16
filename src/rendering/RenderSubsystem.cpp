/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

/*! \brief Init the component. Called on the RENDER thread when the RenderSubsystem is added to the RenderEnvironment */
void RenderSubsystem::ComponentInit()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(!(m_is_initialized.Get(MemoryOrder::ACQUIRE) & g_render_thread));

    Init();

    m_is_initialized.BitOr(uint32(g_render_thread), MemoryOrder::RELEASE);
}

/*! \brief Update data for the component. Called from GAME thread. */
void RenderSubsystem::ComponentUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_is_initialized.Get(MemoryOrder::ACQUIRE) & g_render_thread);

    if (!(m_is_initialized.Get(MemoryOrder::ACQUIRE) & g_game_thread)) {
        InitGame();

        m_is_initialized.BitOr(uint32(g_game_thread), MemoryOrder::RELEASE);
    }

    OnUpdate(delta);
}

/*! \brief Perform rendering. Called from RENDER thread. */
void RenderSubsystem::ComponentRender(IFrame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_is_initialized.Get(MemoryOrder::ACQUIRE) & g_render_thread);

    if (m_render_frame_slicing == 0 || m_render_frame_slicing_counter++ % m_render_frame_slicing == 0) {
        OnRender(frame);
    }
}

void RenderSubsystem::SetComponentIndex(Index index)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (index == m_index) {
        return;
    }

    const auto prev_index = m_index;

    m_index = index;

    if (m_is_initialized.Get(MemoryOrder::ACQUIRE) & g_render_thread) {
        OnComponentIndexChanged(index, prev_index);
    }
}

RenderEnvironment *RenderSubsystem::GetParent() const
{
    Threads::AssertOnThread(g_render_thread);
    
    return m_parent;
}

void RenderSubsystem::SetParent(RenderEnvironment *parent)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_parent = parent;
}

void RenderSubsystem::RemoveFromEnvironment()
{
    HYP_SCOPE;

    struct RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment) : public renderer::RenderCommand
    {
        Weak<RenderSubsystem>   render_subsystem_weak;

        RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)(Weak<RenderSubsystem> render_subsystem_weak)
            : render_subsystem_weak(render_subsystem_weak)
        {
        }

        virtual ~RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)() override = default;

        virtual RendererResult operator()() override
        {
            if (RC<RenderSubsystem> render_subsystem = render_subsystem_weak.Lock()) {
                RenderEnvironment *parent = render_subsystem->GetParent();

                if (parent != nullptr) {
                    parent->RemoveRenderSubsystem(render_subsystem);
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment, WeakRefCountedPtrFromThis());
}

} // namespace hyperion