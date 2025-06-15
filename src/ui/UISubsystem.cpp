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

    m_ui_render_subsystem = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<UIRenderSubsystem>(Name::Unique("UIRenderSubsystem"), m_ui_stage);
}

void UISubsystem::OnRemovedFromWorld()
{
    HYP_SCOPE;

    if (m_ui_render_subsystem != nullptr)
    {
        m_ui_render_subsystem->RemoveFromEnvironment();
        m_ui_render_subsystem.Reset();
    }
}

void UISubsystem::PreUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    m_ui_stage->Update(delta);
}

void UISubsystem::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertThrow(m_ui_stage != nullptr);
    AssertThrow(m_ui_render_subsystem != nullptr);

    if (!m_ui_render_subsystem->IsInitialized())
    {
        return;
    }

    UIRenderCollector& render_collector = m_ui_render_subsystem->GetRenderCollector();
    render_collector.ResetOrdering();

    RenderProxyTracker& render_proxy_tracker = m_ui_render_subsystem->GetRenderProxyTracker();

    m_ui_stage->CollectObjects([&render_collector, &render_proxy_tracker](UIObject* ui_object)
        {
            AssertThrow(ui_object != nullptr);

            const Handle<Node>& node = ui_object->GetNode();
            AssertThrow(node.IsValid());

            const Handle<Entity>& entity = node->GetEntity();

            MeshComponent& mesh_component = node->GetScene()->GetEntityManager()->GetComponent<MeshComponent>(entity);

            if (!mesh_component.proxy)
            {
                return;
            }

            // @TODO Include a way to determine the parent tree of the UI Object because some objects will
            // have the same depth but should be rendered in a different order.
            render_collector.PushRenderProxy(render_proxy_tracker, *mesh_component.proxy, ui_object->GetComputedDepth());
        },
        /* only_visible */ true);

    render_collector.PushUpdatesToRenderThread(render_proxy_tracker, m_ui_render_subsystem->GetFramebuffer());
}

void UISubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
}

void UISubsystem::OnSceneDetached(const Handle<Scene>& scene)
{
}

} // namespace hyperion
