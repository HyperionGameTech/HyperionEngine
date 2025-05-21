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
#include <rendering/RenderScene.hpp>
#include <rendering/RenderEnvironment.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UISubsystem::UISubsystem(const RC<UIStage> &ui_stage)
    : m_ui_stage(ui_stage)
{
}

UISubsystem::~UISubsystem()
{
}

void UISubsystem::Initialize()
{
    HYP_SCOPE;

    AssertThrow(m_ui_stage != nullptr);

    m_ui_stage->Init();
}

void UISubsystem::Shutdown()
{
    HYP_SCOPE;

    for (const Pair<Scene *, RC<UIRenderSubsystem>> &it : m_ui_render_subsystems) {
        it.second->RemoveFromEnvironment();
    }

    m_ui_render_subsystems.Clear();
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

    for (const Pair<Scene *, RC<UIRenderSubsystem>> &it : m_ui_render_subsystems) {
        RC<UIRenderSubsystem> ui_render_subsystem = it.second;
        AssertThrow(ui_render_subsystem != nullptr);

        if (!ui_render_subsystem->IsInitialized()) {
            continue;
        }

        UIRenderCollector &render_collector = ui_render_subsystem->GetRenderCollector();
        render_collector.ResetOrdering();

        RenderProxyTracker &render_proxy_tracker = ui_render_subsystem->GetRenderProxyTracker();

        m_ui_stage->CollectObjects([&render_collector, &render_proxy_tracker](UIObject *ui_object)
        {
            AssertThrow(ui_object != nullptr);

            const Handle<Node> &node = ui_object->GetNode();
            AssertThrow(node.IsValid());

            const Handle<Entity> &entity = node->GetEntity();

            MeshComponent &mesh_component = node->GetScene()->GetEntityManager()->GetComponent<MeshComponent>(entity);

            if (!mesh_component.proxy) {
                return;
            }

            // @TODO Include a way to determine the parent tree of the UI Object because some objects will
            // have the same depth but should be rendered in a different order.
            render_collector.PushRenderProxy(render_proxy_tracker, *mesh_component.proxy, ui_object->GetComputedDepth());
        }, /* only_visible */ true);

        render_collector.PushUpdatesToRenderThread(render_proxy_tracker, ui_render_subsystem->GetFramebuffer());
    }
}

void UISubsystem::OnSceneAttached(const Handle<Scene> &scene)
{
    // Scene must be foreground and not detached or UI
    if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::DETACHED | SceneFlags::UI)) != SceneFlags::FOREGROUND) {
        return;
    }

    RC<UIRenderSubsystem> ui_render_subsystem = scene->GetRenderResource().GetEnvironment()->AddRenderSubsystem<UIRenderSubsystem>(Name::Unique("UIRenderSubsystem"), m_ui_stage);
    m_ui_render_subsystems.Insert(scene.Get(), std::move(ui_render_subsystem));
}

void UISubsystem::OnSceneDetached(const Handle<Scene> &scene)
{
    auto it = m_ui_render_subsystems.Find(scene.Get());

    if (it == m_ui_render_subsystems.End()) {
        return;
    }

    RC<UIRenderSubsystem> ui_render_subsystem = it->second;
    AssertThrow(ui_render_subsystem != nullptr);

    ui_render_subsystem->RemoveFromEnvironment();

    m_ui_render_subsystems.Erase(it);
}

} // namespace hyperion
