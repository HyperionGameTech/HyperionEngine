/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <scene/ecs/components/MeshComponent.hpp>

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UISubsystem::UISubsystem(const Handle<UIStage>& ui_stage)
    : m_ui_stage(ui_stage)
{
}

UISubsystem::~UISubsystem()
{
}

void UISubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    AssertThrow(m_ui_stage != nullptr);

    InitObject(m_ui_stage);

    m_ui_render_subsystem = GetWorld()->AddSubsystem<UIRenderSubsystem>(m_ui_stage);
}

void UISubsystem::OnRemovedFromWorld()
{
    HYP_SCOPE;

    if (m_ui_render_subsystem != nullptr)
    {
        GetWorld()->RemoveSubsystem(m_ui_render_subsystem);
    }
}

void UISubsystem::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertThrow(m_ui_stage != nullptr);
    AssertThrow(m_ui_render_subsystem != nullptr);
}

void UISubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
}

void UISubsystem::OnSceneDetached(const Handle<Scene>& scene)
{
}

} // namespace hyperion
