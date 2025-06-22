/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/debug/DebugDrawer.hpp>

#include <scene/View.hpp>
#include <scene/Texture.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderView

RenderView::RenderView(View* view)
    : m_view(view),
      m_viewport(view ? view->GetViewport() : Viewport {}),
      m_priority(view ? view->GetPriority() : 0)
{
}

RenderView::~RenderView()
{
}

void RenderView::Initialize_Internal()
{
    HYP_SCOPE;

    if (m_view)
    {
        AssertThrow(m_view->GetCamera().IsValid());

        m_render_camera = TResourceHandle<RenderCamera>(m_view->GetCamera()->GetRenderResource());
    }
}

void RenderView::Destroy_Internal()
{
    HYP_SCOPE;

    m_render_camera.Reset();
}

GBuffer* RenderView::GetGBuffer() const
{
    if (!m_view)
    {
        return nullptr;
    }

    return m_view->GetOutputTarget().GetGBuffer();
}

void RenderView::Update_Internal()
{
    HYP_SCOPE;
}

void RenderView::CreateRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
}

void RenderView::DestroyRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
}

void RenderView::SetViewport(const Viewport& viewport)
{
    HYP_SCOPE;

    Execute([this, viewport]()
        {
            if (m_viewport == viewport)
            {
                return;
            }

            m_viewport = viewport;

            if (IsInitialized() && (m_view->GetFlags() & ViewFlags::GBUFFER))
            {
            }
        });
}

void RenderView::SetPriority(int priority)
{
    HYP_SCOPE;

    Execute([this, priority]()
        {
            m_priority = priority;
        });
}

void RenderView::PreRender(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(IsInitialized());

    // if (m_post_processing)
    // {
    //     m_post_processing->PerformUpdates();
    // }
}

void RenderView::PostRender(FrameBase* frame)
{
}

#pragma endregion RenderView

} // namespace hyperion
