/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Scene.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

SceneRenderResources::SceneRenderResources(Scene *scene)
    : m_scene(scene)
{
    m_environment = CreateObject<RenderEnvironment>(m_scene);
    InitObject(m_environment);
}

SceneRenderResources::~SceneRenderResources() = default;

void SceneRenderResources::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void SceneRenderResources::Destroy_Internal()
{
    HYP_SCOPE;
}

void SceneRenderResources::Update_Internal()
{
    HYP_SCOPE;
}

void SceneRenderResources::UpdateBufferData()
{
    HYP_SCOPE;

    m_buffer_data.frame_counter = m_environment->GetFrameCounter();
    m_buffer_data.enabled_render_subsystems_mask = m_environment->GetEnabledRenderSubsystemsMask();

    *static_cast<SceneShaderData *>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void SceneRenderResources::SetBufferData(const SceneShaderData &buffer_data)
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

GPUBufferHolderBase *SceneRenderResources::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->scenes.Get();
}

namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, ScenesBuffer, 1, sizeof(SceneShaderData), true);

} // namespace renderer
} // namespace hyperion
