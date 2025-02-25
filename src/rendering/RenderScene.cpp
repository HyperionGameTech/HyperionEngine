/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderScene.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

SceneRenderResource::SceneRenderResource(Scene *scene)
    : m_scene(scene)
{
    m_environment = CreateObject<RenderEnvironment>(m_scene);
    InitObject(m_environment);
}

SceneRenderResource::~SceneRenderResource() = default;

void SceneRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void SceneRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void SceneRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

void SceneRenderResource::UpdateBufferData()
{
    HYP_SCOPE;

    m_buffer_data.frame_counter = m_environment->GetFrameCounter();
    m_buffer_data.enabled_render_subsystems_mask = m_environment->GetEnabledRenderSubsystemsMask();

    *static_cast<SceneShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void SceneRenderResource::SetBufferData(const SceneShaderData &buffer_data)
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

GPUBufferHolderBase *SceneRenderResource::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->scenes.Get();
}

namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, ScenesBuffer, 1, sizeof(SceneShaderData), true);

} // namespace renderer
} // namespace hyperion
