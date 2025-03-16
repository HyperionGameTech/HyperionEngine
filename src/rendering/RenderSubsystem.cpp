/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

void RenderSubsystem::SetParent(RenderEnvironment *parent)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_parent = parent;
}

void RenderSubsystem::RemoveFromEnvironment()
{
    if (m_parent != nullptr) {
        m_parent->RemoveRenderSubsystem(this);
    }
}

} // namespace hyperion