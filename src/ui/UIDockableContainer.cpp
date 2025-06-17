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
{
}

#pragma endregion UIDockableItem

#pragma region UIDockableContainer

UIDockableContainer::UIDockableContainer()
{
}

void UIDockableContainer::Init()
{
    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++)
    {
        m_dockable_items[i] = CreateUIObject<UIDockableItem>(CreateNameFromDynamicString(ANSIString("DockableItems_") + ANSIString::ToString(i)), Vec2i { 0, 0 }, UIObjectSize());
    }

    UIPanel::Init();

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++)
    {
        UIObject::AddChildUIObject(m_dockable_items[i]);
    }
}

void UIDockableContainer::AddChildUIObject(const Handle<UIObject>& ui_object)
{
    HYP_SCOPE;

    if (!ui_object)
    {
        return;
    }

    UIDockableItemPosition position = UIDockableItemPosition::CENTER;

    if (const NodeTag& side_node_tag = ui_object->GetNodeTag("side"); side_node_tag.IsValid())
    {
        Optional<const String&> side_opt = side_node_tag.data.TryGet<String>();

        if (side_opt.HasValue())
        {
            const auto it = g_dockable_item_position_map.Find(*side_opt);

            if (it != g_dockable_item_position_map.End())
            {
                position = it->second;
            }
        }
    }

    if (position == UIDockableItemPosition::UNDOCKED)
    {
        UIPanel::AddChildUIObject(ui_object);
    }
    else
    {
        m_dockable_items[uint32(position)]->AddChildUIObject(ui_object);
    }

    UpdateSize(true);
}

bool UIDockableContainer::RemoveChildUIObject(UIObject* ui_object)
{
    HYP_SCOPE;

    if (!ui_object)
    {
        return false;
    }

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++)
    {
        if (m_dockable_items[i]->RemoveChildUIObject(ui_object))
        {
            UpdateSize(true);
            return true;
        }
    }

    return UIPanel::RemoveChildUIObject(ui_object);
}

void UIDockableContainer::AddChildUIObject(const Handle<UIObject>& ui_object, UIDockableItemPosition position)
{
    HYP_SCOPE;

    m_dockable_items[uint32(position)]->AddChildUIObject(ui_object);

    UpdateSize(true);
}

void UIDockableContainer::UpdateSize_Internal(bool update_children)
{
    HYP_SCOPE;

    UIPanel::UpdateSize_Internal(update_children);

    if (update_children)
    {
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
}

#pragma region UIDockableContainer

} // namespace hyperion
