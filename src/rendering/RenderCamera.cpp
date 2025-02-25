/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCamera.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/math/Matrix4.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region CameraRenderResource

CameraRenderResource::CameraRenderResource(Camera *camera)
    : m_camera(camera),
      m_buffer_data { }
{
}

CameraRenderResource::CameraRenderResource(CameraRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_camera(other.m_camera),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_camera = nullptr;
}

CameraRenderResource::~CameraRenderResource() = default;

void CameraRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void CameraRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void CameraRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *CameraRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->cameras.Get();
}

void CameraRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<CameraShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void CameraRenderResource::SetBufferData(const CameraShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (IsInitialized()) {
            UpdateBufferData();
        }
    });
}

void CameraRenderResource::EnqueueBind()
{
    HYP_SCOPE;

    Execute([this]()
    {
        g_engine->GetRenderState()->BindCamera(GetCamera());
    }, /* force_render_thread */ true);
}

void CameraRenderResource::EnqueueUnbind()
{
    HYP_SCOPE;

    Execute([this]()
    {
        g_engine->GetRenderState()->UnbindCamera(GetCamera());
    }, /* force_render_thread */ true);
}

#pragma endregion CameraRenderResource

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Scene, CamerasBuffer, 1, sizeof(CameraShaderData), true);

} // namespace renderer
} // namespace hyperion