/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

RenderEnvironment* RenderSubsystem::GetParent() const
{
    Threads::AssertOnThread(g_render_thread);

    return m_parent;
}

void RenderSubsystem::SetParent(RenderEnvironment* parent)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_parent = parent;
}

void RenderSubsystem::RemoveFromEnvironment()
{
    HYP_SCOPE;

    // struct RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)
    //     : public RenderCommand
    // {
    //     Handle<RenderSubsystem> render_subsystem;

    //     RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)(Handle<RenderSubsystem>&& render_subsystem)
    //         : render_subsystem(render_subsystem)
    //     {
    //     }

    //     virtual ~RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment)() override = default;

    //     virtual RendererResult operator()() override
    //     {
    //         if (render_subsystem)
    //         {
    //             RenderEnvironment* parent = render_subsystem->GetParent();

    //             if (parent != nullptr)
    //             {
    //                 parent->RemoveRenderSubsystem(render_subsystem);
    //             }
    //         }

    //         HYPERION_RETURN_OK;
    //     }
    // };

    // PUSH_RENDER_COMMAND(RemoveRenderSubsystemFromEnvironment, HandleFromThis());
}

} // namespace hyperion