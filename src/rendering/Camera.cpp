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

CameraRenderResources::CameraRenderResources(const WeakHandle<Camera> &camera_weak)
    : m_camera_weak(camera_weak),
      m_buffer_data { }
{
}

CameraRenderResources::CameraRenderResources(CameraRenderResources &&other) noexcept
    : RenderResourcesBase(static_cast<RenderResourcesBase &&>(other)),
      m_camera_weak(std::move(other.m_camera_weak)),
      m_buffer_data(std::move(other.m_buffer_data))
{
}

CameraRenderResources::~CameraRenderResources()
{
    Destroy();
}

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

    UpdateBufferData();
}

uint32 CameraRenderResources::AcquireBufferIndex() const
{
    HYP_SCOPE;

    return g_engine->GetRenderData()->cameras->AcquireIndex();
}

void CameraRenderResources::ReleaseBufferIndex(uint32 buffer_index) const
{
    HYP_SCOPE;

    g_engine->GetRenderData()->cameras->ReleaseIndex(buffer_index);
}

void CameraRenderResources::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    g_engine->GetRenderData()->cameras->Set(m_buffer_index, m_buffer_data);
}

void CameraRenderResources::SetBufferData(const CameraShaderData &buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
    {
        m_buffer_data = buffer_data;

        if (m_is_initialized) {
            SetNeedsUpdate();
        }
    });
}

// @TODO Refactor to not update the global buffer data, instead it should just update m_buffer_data and call SetNeedsUpdate
void CameraRenderResources::ApplyJitter()
{
    HYP_SCOPE;

    AssertThrow(m_buffer_index != ~0u);

    Vec4f jitter = Vec4f::Zero();

    const uint frame_counter = g_engine->GetRenderState().frame_counter + 1;

    static const float jitter_scale = 0.25f;

    if (m_buffer_data.projection[3][3] < MathUtil::epsilon_f) {
        Matrix4::Jitter(frame_counter, m_buffer_data.dimensions.x, m_buffer_data.dimensions.y, jitter);

        m_buffer_data.jitter = jitter * jitter_scale;

        g_engine->GetRenderData()->cameras->Set(m_buffer_index, m_buffer_data);
    }
}

#pragma endregion CameraRenderResources

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Scene, CamerasBuffer, 1, sizeof(CameraShaderData), true);

} // namespace renderer
} // namespace hyperion