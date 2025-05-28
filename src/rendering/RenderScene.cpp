/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <scene/World.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

RenderScene::RenderScene(Scene* scene)
    : m_scene(scene)
{
    m_environment = CreateObject<RenderEnvironment>();
    InitObject(m_environment);
}

RenderScene::~RenderScene() = default;

void RenderScene::SetCameraRenderResourceHandle(const TResourceHandle<RenderCamera>& render_camera)
{
    Execute([this, render_camera]()
        {
            m_render_camera = std::move(render_camera);
        });
}

void RenderScene::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderScene::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderScene::Update_Internal()
{
    HYP_SCOPE;
}

void RenderScene::UpdateBufferData()
{
    HYP_SCOPE;

    m_buffer_data.frame_counter = m_environment->GetFrameCounter();
    m_buffer_data.enabled_render_subsystems_mask = m_environment->GetEnabledRenderSubsystemsMask();

    *static_cast<SceneShaderData*>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

void RenderScene::SetBufferData(const SceneShaderData& buffer_data)
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

GPUBufferHolderBase* RenderScene::GetGPUBufferHolder() const
{
    return g_engine->GetRenderData()->scenes;
}

namespace renderer {

HYP_DESCRIPTOR_SSBO(Global, ScenesBuffer, 1, sizeof(SceneShaderData), true);

} // namespace renderer
} // namespace hyperion
