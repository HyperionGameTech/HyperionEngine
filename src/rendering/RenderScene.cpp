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

GpuBufferHolderBase* RenderScene::GetGpuBufferHolder() const
{
    return nullptr;
}

} // namespace hyperion
