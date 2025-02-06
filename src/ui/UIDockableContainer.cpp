/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDockableContainer.hpp>
#include <ui/UIStage.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

static const HashMap<String, UIDockableItemPosition> g_dockable_item_position_map = {
    { "left", UIDockableItemPosition::LEFT },
    { "right", UIDockableItemPosition::RIGHT },
    { "top", UIDockableItemPosition::TOP },
    { "bottom", UIDockableItemPosition::BOTTOM },
    { "center", UIDockableItemPosition::CENTER },
    { "undocked", UIDockableItemPosition::UNDOCKED }
};

#pragma region UIDockableItem

UIDockableItem::UIDockableItem()
    : UIPanel(UIObjectType::DOCKABLE_ITEM)
{
}

#pragma endregion UIDockableItem

#pragma region UIDockableContainer

UIDockableContainer::UIDockableContainer()
    : UIPanel(UIObjectType::DOCKABLE_CONTAINER)
{
}

void UIDockableContainer::Init()
{
    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++) {
        m_dockable_items[i] = CreateUIObject<UIDockableItem>(CreateNameFromDynamicString(ANSIString("DockableItems_") + ANSIString::ToString(i)), Vec2i { 0, 0 }, UIObjectSize());
    }

    UIPanel::Init();

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++) {
        UIObject::AddChildUIObject(m_dockable_items[i]);
    }
}

void UIDockableContainer::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return;
    }

    UIDockableItemPosition position = UIDockableItemPosition::CENTER;

    if (const NodeTag &side_node_tag = ui_object->GetNodeTag("side"); side_node_tag.IsValid()) {
        Optional<const String &> side_opt = side_node_tag.data.TryGet<String>();

        if (side_opt.HasValue()) {
            const auto it = g_dockable_item_position_map.Find(*side_opt);
        
            if (it != g_dockable_item_position_map.End()) {
                position = it->second;
            }
        }
    }

    if (position == UIDockableItemPosition::UNDOCKED) {
        UIPanel::AddChildUIObject(ui_object);
    } else {
        m_dockable_items[uint32(position)]->AddChildUIObject(ui_object);
    }

    UpdateSize(true);
}

bool UIDockableContainer::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++) {
        if (m_dockable_items[i]->RemoveChildUIObject(ui_object)) {
            UpdateSize(true);
            return true;
        }
    }

    return UIPanel::RemoveChildUIObject(ui_object);
}

void UIDockableContainer::AddChildUIObject(const RC<UIObject> &ui_object, UIDockableItemPosition position)
{
    HYP_SCOPE;

    m_dockable_items[uint32(position)]->AddChildUIObject(ui_object);

    UpdateSize(true);
}

void UIDockableContainer::UpdateSize_Internal(bool update_children)
{
    HYP_SCOPE;

    UIPanel::UpdateSize_Internal(update_children);

    if (update_children) {
        UpdateLayout();
    }
}

void UIDockableContainer::UpdateLayout()
{
    HYP_SCOPE;

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

    DebugLog(LogType::Debug, "Top aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetLocalAABB().max.z);
    DebugLog(LogType::Debug, "Bottom aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetLocalAABB().max.z);
    DebugLog(LogType::Debug, "Left aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetLocalAABB().max.z);
    DebugLog(LogType::Debug, "Right aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetLocalAABB().max.z);
    DebugLog(LogType::Debug, "Center aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetLocalAABB().max.z);
    
    if (m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren().Any()) DebugLog(LogType::Debug, "Top child aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.z);
    if (m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren().Any()) DebugLog(LogType::Debug, "Bottom child aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.z);
    if (m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren().Any()) DebugLog(LogType::Debug, "Left child aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.z);
    if (m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren().Any()) DebugLog(LogType::Debug, "Right child aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.z);
    if (m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren().Any()) DebugLog(LogType::Debug, "Center child aabb: %f,%f,%f\t%f,%f,%f\n", m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.y, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren()[0]->GetLocalAABB().min.z, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.x, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.y, m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren()[0]->GetLocalAABB().max.z);

    DebugLog(LogType::Debug, "Top num children: %u\n", m_dockable_items[uint32(UIDockableItemPosition::TOP)]->GetNode()->GetChildren().Size());
    DebugLog(LogType::Debug, "Bottom num children: %u\n", m_dockable_items[uint32(UIDockableItemPosition::BOTTOM)]->GetNode()->GetChildren().Size());
    DebugLog(LogType::Debug, "Left num children: %u\n", m_dockable_items[uint32(UIDockableItemPosition::LEFT)]->GetNode()->GetChildren().Size());
    DebugLog(LogType::Debug, "Right num children: %u\n", m_dockable_items[uint32(UIDockableItemPosition::RIGHT)]->GetNode()->GetChildren().Size());
    DebugLog(LogType::Debug, "Center num children: %u\n", m_dockable_items[uint32(UIDockableItemPosition::CENTER)]->GetNode()->GetChildren().Size());
}

#pragma region UIDockableContainer

} // namespace hyperion
