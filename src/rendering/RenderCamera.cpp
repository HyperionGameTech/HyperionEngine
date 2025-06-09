/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/math/Matrix4.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderCamera

RenderCamera::RenderCamera(Camera* camera)
    : m_camera(camera),
      m_buffer_data {}
{
}

RenderCamera::~RenderCamera()
{
}

void RenderCamera::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();

    if (m_framebuffer.IsValid())
    {
        DeferCreate(m_framebuffer);
    }
}

void RenderCamera::Destroy_Internal()
{
    HYP_SCOPE;

    SafeRelease(std::move(m_framebuffer));
}

void RenderCamera::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase* RenderCamera::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->cameras;
}

void RenderCamera::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<CameraShaderData*>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void RenderCamera::SetBufferData(const CameraShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            m_buffer_data = buffer_data;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

void RenderCamera::SetFramebuffer(const FramebufferRef& framebuffer)
{
    HYP_SCOPE;

    Execute([this, framebuffer]()
        {
            SafeRelease(std::move(m_framebuffer));

            m_framebuffer = framebuffer;

            if (IsInitialized() && m_framebuffer.IsValid())
            {
                DeferCreate(m_framebuffer);
            }
        });
}

void RenderCamera::EnqueueBind()
{
    HYP_SCOPE;

    Execute([this]()
        {
            g_engine->GetRenderState()->BindCamera(TResourceHandle<RenderCamera>(*this));
        },
        /* force_render_thread */ true);
}

void RenderCamera::EnqueueUnbind()
{
    HYP_SCOPE;

    Execute([this]()
        {
            g_engine->GetRenderState()->UnbindCamera(this);
        },
        /* force_render_thread */ true);
}

void RenderCamera::ApplyJitter(const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    static const float jitter_scale = 0.25f;

    AssertThrow(m_buffer_index != ~0u);

    const uint32 frame_counter = render_setup.world->GetBufferData().frame_counter + 1;

    CameraShaderData& buffer_data = *static_cast<CameraShaderData*>(m_buffer_address);

    if (buffer_data.projection[3][3] < MathUtil::epsilon_f)
    {
        Vec4f jitter = Vec4f::Zero();

        Matrix4::Jitter(frame_counter, buffer_data.dimensions.x, buffer_data.dimensions.y, jitter);

        buffer_data.jitter = jitter * jitter_scale;

        g_engine->GetRenderData()->cameras->MarkDirty(m_buffer_index);
    }
}

#pragma endregion RenderCamera

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Global, CamerasBuffer, 1, sizeof(CameraShaderData), true);

} // namespace renderer
} // namespace hyperion