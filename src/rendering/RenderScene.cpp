/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <scene/World.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

RenderScene::RenderScene(Scene* scene)
    : m_scene(scene)
{
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
}

void RenderScene::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderScene::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase* RenderScene::GetGPUBufferHolder() const
{
    return nullptr;
}

} // namespace hyperion
