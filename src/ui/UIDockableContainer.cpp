/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDockableContainer.hpp>
#include <ui/UIStage.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region UIDockableItem

UIDockableItem::UIDockableItem(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::DOCKABLE_ITEM)
{
}

#pragma endregion UIDockableItem

#pragma region UIDockableContainer

UIDockableContainer::UIDockableContainer(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::DOCKABLE_CONTAINER),
      m_flow(UIDockableContainerFlow::HORIZONTAL)
{
    AssertThrow(m_parent != nullptr);

    m_left_container = m_parent->CreateUIObject<UIPanel>(HYP_NAME(DockableItems_Side0), Vec2i { 0, 0 }, UIObjectSize({ 25, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    UIObject::AddChildUIObject(m_left_container);

    m_center_container = m_parent->CreateUIObject<UIPanel>(HYP_NAME(DockableItems_Center), Vec2i { 0, 0 }, UIObjectSize({ 50, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    UIObject::AddChildUIObject(m_center_container);

    m_right_container = m_parent->CreateUIObject<UIPanel>(HYP_NAME(DockableItems_Side1), Vec2i { 0, 0 }, UIObjectSize({ 25, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    UIObject::AddChildUIObject(m_right_container);
}

void UIDockableContainer::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();
}

void UIDockableContainer::AddChildUIObject(UIObject *ui_object, UIDockableItemPosition position)
{
    switch (position) {
    case UIDockableItemPosition::LEFT:
        m_left_container->AddChildUIObject(ui_object);

        break;
    case UIDockableItemPosition::RIGHT:
        m_right_container->AddChildUIObject(ui_object);

        break;
    case UIDockableItemPosition::CENTER:
        m_center_container->AddChildUIObject(ui_object);

        break;
    }
}

void UIDockableContainer::UpdateSize(bool update_children)
{
    UIPanel::UpdateSize(update_children);

    UpdateLayout();
}

void UIDockableContainer::UpdateLayout()
{
    const Vec2i container_size = GetActualSize();

    switch (m_flow) {
    case UIDockableContainerFlow::HORIZONTAL:
        m_left_container->SetPosition(Vec2i { 0, 0 });
        m_left_container->SetSize(UIObjectSize({ 25, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

        m_right_container->SetSize(UIObjectSize({ 25, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
        m_right_container->SetPosition(Vec2i { container_size.x - m_right_container->GetActualSize().x, 0 });

        m_center_container->SetPosition(Vec2i { m_left_container->GetActualSize().x, 0 });
        m_center_container->SetSize(UIObjectSize({ container_size.x - (m_left_container->GetActualSize().x + m_right_container->GetActualSize().x), UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));

        break;
    case UIDockableContainerFlow::VERTICAL:
        m_left_container->SetPosition(Vec2i { 0, 0 });
        m_left_container->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PERCENT }));

        m_right_container->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PERCENT }));
        m_right_container->SetPosition(Vec2i { 0, container_size.y - m_right_container->GetActualSize().y });

        m_center_container->SetPosition(Vec2i { 0, m_left_container->GetActualSize().y });
        m_center_container->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { container_size.x - (m_left_container->GetActualSize().x + m_right_container->GetActualSize().x), UIObjectSize::PIXEL }));

        break;
    }
}

#pragma region UIDockableContainer

} // namespace hyperion
