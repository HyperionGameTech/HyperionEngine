/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Camera.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <math/Matrix4.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region CameraRenderResources

CameraRenderResources::CameraRenderResources(Camera *camera)
    : m_camera(camera),
      m_buffer_data { }
{
}

CameraRenderResources::CameraRenderResources(CameraRenderResources &&other) noexcept
    : RenderResourcesBase(static_cast<RenderResourcesBase &&>(other)),
      m_camera(other.m_camera),
      m_buffer_data(std::move(other.m_buffer_data))
{
    other.m_camera = nullptr;
}

CameraRenderResources::~CameraRenderResources() = default;

void CameraRenderResources::Initialize()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void CameraRenderResources::Destroy()
{
    HYP_SCOPE;
}

void CameraRenderResources::Update()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *CameraRenderResources::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->cameras.Get();
}

void CameraRenderResources::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<CameraShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void CameraRenderResources::SetBufferData(const CameraShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (m_is_initialized) {
            UpdateBufferData();
        }
    });
}

#pragma endregion CameraRenderResources

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Scene, CamerasBuffer, 1, sizeof(CameraShaderData), true);

} // namespace renderer
} // namespace hyperion