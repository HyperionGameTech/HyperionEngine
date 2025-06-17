/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderEnvProbe.hpp>

#include <scene/camera/Camera.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(RenderState, Rendering);

RenderState::RenderState()
{
}

RenderState::~RenderState() = default;

void RenderState::Init()
{
    SetReady(true);
}

void RenderState::BindCamera(const TResourceHandle<RenderCamera>& resource_handle)
{
    if (!resource_handle)
    {
        return;
    }

    Threads::AssertOnThread(g_render_thread);

    // Allow multiple of the same so we can always override the topmost camera
    camera_bindings.PushBack(resource_handle);
}

void RenderState::UnbindCamera(const RenderCamera* render_camera)
{
    if (!render_camera)
    {
        return;
    }

    Threads::AssertOnThread(g_render_thread);

    for (auto it = camera_bindings.Begin(); it != camera_bindings.End();)
    {
        if ((*it).Get() == render_camera)
        {
            it = camera_bindings.Erase(it);

            // Stop iterating at first match
            break;
        }
        else
        {
            ++it;
        }
    }
}

const TResourceHandle<RenderCamera>& RenderState::GetActiveCamera() const
{
    Threads::AssertOnThread(g_render_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    static const TResourceHandle<RenderCamera> empty;

    return camera_bindings.Any()
        ? camera_bindings.Back()
        : empty;
}

const TResourceHandle<RenderLight>& RenderState::GetActiveLight() const
{
    Threads::AssertOnThread(g_render_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    static const TResourceHandle<RenderLight> empty;

    return light_bindings.Any()
        ? light_bindings.Top()
        : empty;
}

void RenderState::SetActiveLight(const TResourceHandle<RenderLight>& light_resource_handle)
{
    Threads::AssertOnThread(g_render_thread);

    light_bindings.Push(light_resource_handle);
}

} // namespace hyperion