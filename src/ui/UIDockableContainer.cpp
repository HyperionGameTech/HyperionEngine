/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDockableContainer.hpp>
#include <ui/UIStage.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

static const HashMap<String, UIDockableItemPosition> g_dockableItemPositionMap = {
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
        m_dockableItems[i] = CreateUIObject<UIDockableItem>(CreateNameFromDynamicString(ANSIString("DockableItems_") + ANSIString::ToString(i)), Vec2i { 0, 0 }, UIObjectSize());
    }

    UIPanel::Init();

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++)
    {
        UIObject::AddChildUIObject(m_dockableItems[i]);
    }
}

void UIDockableContainer::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return;
    }

    UIDockableItemPosition position = UIDockableItemPosition::CENTER;

    if (const NodeTag& sideNodeTag = uiObject->GetNodeTag("side"); sideNodeTag.IsValid())
    {
        Optional<const String&> sideOpt = sideNodeTag.data.TryGet<String>();

        if (sideOpt.HasValue())
        {
            const auto it = g_dockableItemPositionMap.Find(*sideOpt);

            if (it != g_dockableItemPositionMap.End())
            {
                position = it->second;
            }
        }
    }

    if (position == UIDockableItemPosition::UNDOCKED)
    {
        UIPanel::AddChildUIObject(uiObject);
    }
    else
    {
        m_dockableItems[uint32(position)]->AddChildUIObject(uiObject);
    }

    UpdateSize(true);
}

bool UIDockableContainer::RemoveChildUIObject(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return false;
    }

    for (uint32 i = 0; i < uint32(UIDockableItemPosition::MAX); i++)
    {
        if (m_dockableItems[i]->RemoveChildUIObject(uiObject))
        {
            UpdateSize(true);
            return true;
        }
    }

    return UIPanel::RemoveChildUIObject(uiObject);
}

void UIDockableContainer::AddChildUIObject(const Handle<UIObject>& uiObject, UIDockableItemPosition position)
{
    HYP_SCOPE;

    m_dockableItems[uint32(position)]->AddChildUIObject(uiObject);

    UpdateSize(true);
}

void UIDockableContainer::UpdateSize_Internal(bool updateChildren)
{
    HYP_SCOPE;

    UIPanel::UpdateSize_Internal(updateChildren);

    if (updateChildren)
    {
        UpdateLayout();
    }
}

void UIDockableContainer::UpdateLayout()
{
    HYP_SCOPE;

    const Vec2i containerSize = GetActualSize();

    // Top
    m_dockableItems[uint32(UIDockableItemPosition::TOP)]->SetPosition(Vec2i { 0, 0 });
    m_dockableItems[uint32(UIDockableItemPosition::TOP)]->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    // Bottom
    m_dockableItems[uint32(UIDockableItemPosition::BOTTOM)]->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    m_dockableItems[uint32(UIDockableItemPosition::BOTTOM)]->SetPosition(Vec2i { 0, containerSize.y - m_dockableItems[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y });

    // Left
    m_dockableItems[uint32(UIDockableItemPosition::LEFT)]->SetPosition(Vec2i { 0, m_dockableItems[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y });
    m_dockableItems[uint32(UIDockableItemPosition::LEFT)]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { containerSize.y - (m_dockableItems[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y + m_dockableItems[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y), UIObjectSize::PIXEL }));

    // Right
    m_dockableItems[uint32(UIDockableItemPosition::RIGHT)]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { containerSize.y - (m_dockableItems[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y + m_dockableItems[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y), UIObjectSize::PIXEL }));
    m_dockableItems[uint32(UIDockableItemPosition::RIGHT)]->SetPosition(Vec2i { containerSize.x - m_dockableItems[uint32(UIDockableItemPosition::RIGHT)]->GetActualSize().x, m_dockableItems[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y });

    // Center
    m_dockableItems[uint32(UIDockableItemPosition::CENTER)]->SetPosition(Vec2i { m_dockableItems[uint32(UIDockableItemPosition::LEFT)]->GetActualSize().x, m_dockableItems[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y });
    m_dockableItems[uint32(UIDockableItemPosition::CENTER)]->SetSize(UIObjectSize({ containerSize.x - (m_dockableItems[uint32(UIDockableItemPosition::LEFT)]->GetActualSize().x + m_dockableItems[uint32(UIDockableItemPosition::RIGHT)]->GetActualSize().x), UIObjectSize::PIXEL }, { containerSize.y - (m_dockableItems[uint32(UIDockableItemPosition::TOP)]->GetActualSize().y + m_dockableItems[uint32(UIDockableItemPosition::BOTTOM)]->GetActualSize().y), UIObjectSize::PIXEL }));
}

#pragma region UIDockableContainer

} // namespace hyperion
