/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <scene/World.hpp>
#include <scene/Node.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <scene/ecs/components/MeshComponent.hpp>

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderWorld.hpp>
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

    m_ui_render_subsystem = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<UIRenderSubsystem>(Name::Unique("UIRenderSubsystem"), m_ui_stage);
}

void UISubsystem::Shutdown()
{
    HYP_SCOPE;

    m_ui_render_subsystem->RemoveFromEnvironment();
    m_ui_render_subsystem.Reset();
}

void UISubsystem::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    m_ui_stage->Update(delta);

    if (m_ui_render_subsystem->IsInitialized()) {
        UIRenderCollector &render_collector = m_ui_render_subsystem->GetRenderCollector();

        render_collector.ResetOrdering();

        m_ui_stage->CollectObjects([&render_collector](UIObject *ui_object)
        {
            AssertThrow(ui_object != nullptr);

            const NodeProxy &node = ui_object->GetNode();
            AssertThrow(node.IsValid());

            const Handle<Entity> &entity = node->GetEntity();

            MeshComponent *mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
            AssertThrow(mesh_component != nullptr);
            AssertThrow(mesh_component->proxy != nullptr);

            render_collector.PushEntityToRender(entity, *mesh_component->proxy, ui_object->GetComputedDepth());
        }, /* only_visible */ true);

        render_collector.PushUpdatesToRenderThread(m_ui_stage->GetScene()->GetCamera()->GetFramebuffer());
    }
}

} // namespace hyperion
