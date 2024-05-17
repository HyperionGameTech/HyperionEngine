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
    : UIPanel(parent, std::move(node_proxy), UIObjectType::DOCKABLE_CONTAINER)
{
    AssertThrow(m_parent != nullptr);

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++) {
        m_dockable_items[i] = m_parent->CreateUIObject<UIDockableItem>(CreateNameFromDynamicString(ANSIString("DockableItems_") + ANSIString::ToString(i)), Vec2i { 0, 0 }, UIObjectSize());
    }

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++) {
        UIObject::AddChildUIObject(m_dockable_items[i]);
    }
}

void UIDockableContainer::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();
}

void UIDockableContainer::AddChildUIObject(UIObject *ui_object, UIDockableItemPosition position)
{
    m_dockable_items[uint32(position)]->AddChildUIObject(ui_object);

    UpdateSize(true);
}

void UIDockableContainer::UpdateSize(bool update_children)
{
    UIPanel::UpdateSize(update_children);

    UpdateLayout();
}

void UIDockableContainer::UpdateLayout()
{
    const Vec2i container_size = GetActualSize();

    // Top
    m_dockable_items[uint32(UIDockableItemPosition::TOP)]->SetPosition(Vec2i { 0, 0 });
    m_dockable_items[uint32(UIDockableItemPosition::TOP)]->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    // Bottom
    m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->SetPosition(Vec2i { 0, container_size.y - m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y });

    // Left
    m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->SetPosition(Vec2i { 0, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y });
    m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { container_size.y - (m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y + m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y), UIObjectSize::PIXEL }));

    // Right
    m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { container_size.y - (m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y + m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y), UIObjectSize::PIXEL }));
    m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->SetPosition(Vec2i { container_size.x - m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetActualSize().x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y });

    // Center
    m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->SetPosition(Vec2i { m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetActualSize().x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y });
    m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->SetSize(UIObjectSize({ container_size.x - (m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetActualSize().x + m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetActualSize().x), UIObjectSize::PIXEL }, { container_size.y - (m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y + m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y), UIObjectSize::PIXEL }));


    DebugLog(LogType::Debug, "Container size: %d,%d\n", container_size.x, container_size.y);
    DebugLog(LogType::Debug, "Top size: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y);
    DebugLog(LogType::Debug, "Bottom size: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y);
    DebugLog(LogType::Debug, "Left size: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetActualSize().x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetActualSize().y);
    DebugLog(LogType::Debug, "Right size: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetActualSize().x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetActualSize().y);
    DebugLog(LogType::Debug, "Center size: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetActualSize().x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetActualSize().y);

    DebugLog(LogType::Debug, "Container position: %d,%d\n", GetPosition().x, GetPosition().y);
    DebugLog(LogType::Debug, "Top position: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetPosition().x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetPosition().y);
    DebugLog(LogType::Debug, "Bottom position: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetPosition().x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetPosition().y);
    DebugLog(LogType::Debug, "Left position: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetPosition().x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetPosition().y);
    DebugLog(LogType::Debug, "Right position: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetPosition().x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetPosition().y);
    DebugLog(LogType::Debug, "Center position: %d,%d\n", m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetPosition().x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetPosition().y);

    DebugLog(LogType::Debug, "Top aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetWorldAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetWorldAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetWorldAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetWorldAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetWorldAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetWorldAABB().max.z);
    DebugLog(LogType::Debug, "Bottom aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetWorldAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetWorldAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetWorldAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetWorldAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetWorldAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetWorldAABB().max.z);
    DebugLog(LogType::Debug, "Left aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetWorldAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetWorldAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetWorldAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetWorldAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetWorldAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetWorldAABB().max.z);
    DebugLog(LogType::Debug, "Right aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetWorldAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetWorldAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetWorldAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetWorldAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetWorldAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetWorldAABB().max.z);
    DebugLog(LogType::Debug, "Center aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetWorldAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetWorldAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetWorldAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetWorldAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetWorldAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetWorldAABB().max.z);
}

#pragma region UIDockableContainer

} // namespace hyperion
