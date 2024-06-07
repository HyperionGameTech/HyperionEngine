/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIMenuBar.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIMenuItem

UIMenuItem::UIMenuItem(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::MENU_ITEM)
{
    SetBorderRadius(0);
    SetPadding({ 5, 0 });
}

void UIMenuItem::Init()
{
    UIPanel::Init();

    RC<UIText> text_element = GetStage()->CreateUIObject<UIText>(CreateNameFromDynamicString(HYP_FORMAT("{}_Text", m_name)), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 12, UIObjectSize::PIXEL }));
    text_element->SetParentAlignment(UIObjectAlignment::CENTER);
    text_element->SetOriginAlignment(UIObjectAlignment::CENTER);
    text_element->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    text_element->SetText(m_text);
    m_text_element = text_element;

    AddChildUIObject(text_element);

    RC<UIPanel> drop_down_menu = GetStage()->CreateUIObject<UIPanel>(CreateNameFromDynamicString(HYP_FORMAT("{}_DropDownMenu", m_name)), Vec2i { 0, 0 }, UIObjectSize({ 150, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));
    drop_down_menu->SetAcceptsFocus(false);
    drop_down_menu->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    drop_down_menu->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    drop_down_menu->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    m_drop_down_menu = drop_down_menu;

    UpdateDropDownMenu();
}

void UIMenuItem::SetText(const String &text)
{
    m_text = text;

    if (m_text_element != nullptr) {
        m_text_element->SetText(m_text);
    }
}

void UIMenuItem::AddDropDownMenuItem(DropDownMenuItem &&item)
{
    m_drop_down_menu_items.PushBack(std::move(item));
    
    UpdateDropDownMenu();
}

void UIMenuItem::SetDropDownMenuItems(Array<DropDownMenuItem> items)
{
    m_drop_down_menu_items = std::move(items);

    UpdateDropDownMenu();
}

DropDownMenuItem *UIMenuItem::GetDropDownMenuItem(Name name)
{
    const auto it = m_drop_down_menu_items.FindIf([name](const DropDownMenuItem &item)
    {
        return item.name == name;
    });

    if (it == m_drop_down_menu_items.End()) {
        return nullptr;
    }

    return it;
}

const DropDownMenuItem *UIMenuItem::GetDropDownMenuItem(Name name) const
{
    return const_cast<UIMenuItem *>(this)->GetDropDownMenuItem(name);
}

Handle<Material> UIMenuItem::GetMaterial() const
{
    return UIPanel::GetMaterial();
}

void UIMenuItem::UpdateDropDownMenu()
{
    AssertThrow(m_drop_down_menu != nullptr);

    if (const NodeProxy &node = m_drop_down_menu->GetNode()) {
        node->RemoveAllChildren();
    }

    if (m_drop_down_menu_items.Empty()) {
        return;
    }

    Vec2i offset = { 0, 0 };

    for (const DropDownMenuItem &item : m_drop_down_menu_items) {
        const Name drop_down_menu_item_name = CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_" + ANSIString(item.name.LookupString()));

        RC<UIButton> drop_down_menu_item = m_drop_down_menu->FindChildUIObject(drop_down_menu_item_name).Cast<UIButton>();

        if (drop_down_menu_item != nullptr) {
            offset += { 0, drop_down_menu_item->GetActualSize().y };

            continue;
        }

        drop_down_menu_item = GetStage()->CreateUIObject<UIButton>(drop_down_menu_item_name, offset, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
        drop_down_menu_item->SetAcceptsFocus(false);
        drop_down_menu_item->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
        drop_down_menu_item->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
        drop_down_menu_item->SetBorderFlags(UIObjectBorderFlags::NONE);
        drop_down_menu_item->SetBorderRadius(0);
        drop_down_menu_item->SetText(item.text);

        drop_down_menu_item->OnClick.Bind([this, name = item.name](const MouseEvent &data) -> UIEventHandlerResult
        {
            HYP_LOG(UI, LogLevel::DEBUG, "OnClick item with name {}", name);

            DropDownMenuItem *item_ptr = GetDropDownMenuItem(name);

            if (item_ptr == nullptr) {
                HYP_LOG(UI, LogLevel::WARNING, "Could not find drop down menu item with name {}", name);

                return UIEventHandlerResult::ERR;
            }

            if (!item_ptr->action.IsValid()) {
                return UIEventHandlerResult::OK;
            }

            item_ptr->action();

            return UIEventHandlerResult::OK; // bubble up to container so it can close
        }).Detach();

        drop_down_menu_item->GetTextElement()->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
        drop_down_menu_item->GetTextElement()->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);

        m_drop_down_menu->AddChildUIObject(drop_down_menu_item);

        offset += { 0, drop_down_menu_item->GetActualSize().y };
    }
}

#pragma endregion UIMenuItem

#pragma region UIMenuBar

UIMenuBar::UIMenuBar(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::MENU_BAR),
      m_selected_menu_item_index(~0u)
{
    SetBorderRadius(0);
    SetPadding({ 5, 2 });
}

void UIMenuBar::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    m_container = GetStage()->CreateUIObject<UIPanel>(HYP_NAME(MenuItemContents), Vec2i { 0, 0 }, UIObjectSize({ 80, UIObjectSize::PIXEL }, { 250, UIObjectSize::PIXEL }));
    m_container->SetBackgroundColor(Color(0xFF0000FFu));
    m_container->SetIsVisible(false);
    m_container->SetBorderFlags(UIObjectBorderFlags::NONE);
    m_container->SetBorderRadius(0);
    // m_container->SetParentAlignment(UIObjectAlignment::BOTTOM_LEFT);
    m_container->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    m_container->SetPadding({ 1, 1 });
    m_container->SetDepth(100);

    // @TODO: OnRemoved_Internal() , remove m_container from stage

    m_container->OnClick.Bind([this](const MouseEvent &data) -> UIEventHandlerResult
    {
        // Hide container on any item clicked
        SetSelectedMenuItemIndex(~0u);
        // Lose focus of the container (otherwise hovering over other menu items will cause the menu strips to reappear)
        Blur();

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    m_container->OnLoseFocus.Bind([this](const MouseEvent &data) -> UIEventHandlerResult
    {
        // First check that any child items aren't focused before hiding the container.
        if (!HasFocus(true)) {
            SetSelectedMenuItemIndex(~0u);
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    GetStage()->AddChildUIObject(m_container);

    // AddChildUIObject(m_container);
}

void UIMenuBar::OnRemoved_Internal()
{
    UIPanel::OnRemoved_Internal();

    if (m_container != nullptr) {
        m_container->RemoveFromParent();
    }
}

void UIMenuBar::SetSelectedMenuItemIndex(uint index)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    if (index == m_selected_menu_item_index) {
        return;
    }

    m_selected_menu_item_index = index;

    m_container->SetIsVisible(false);

    if (const NodeProxy &node = m_container->GetNode()) {
        node->RemoveAllChildren();
    }

    for (uint i = 0; i < m_menu_items.Size(); i++) {
        if (i == m_selected_menu_item_index) {
            continue;
        }

        const RC<UIMenuItem> &menu_item = m_menu_items[i];

        if (!menu_item) {
            continue;
        }

        menu_item->SetFocusState(menu_item->GetFocusState() & ~UIObjectFocusState::TOGGLED);
    }

    if (m_selected_menu_item_index >= m_menu_items.Size()) {
        m_selected_menu_item_index = ~0u;

        return;
    }

    const RC<UIMenuItem> &menu_item = m_menu_items[m_selected_menu_item_index];

    if (!menu_item || !menu_item->GetDropDownMenuElement()) {
        return;
    }

    menu_item->SetFocusState(menu_item->GetFocusState() | UIObjectFocusState::TOGGLED);

    m_container->AddChildUIObject(menu_item->GetDropDownMenuElement());
    m_container->SetSize(UIObjectSize({ menu_item->GetDropDownMenuElement()->GetActualSize().x + m_container->GetPadding().x * 2, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));
    m_container->SetPosition({ menu_item->GetPosition().x, menu_item->GetPosition().y + menu_item->GetActualSize().y });
    m_container->SetIsVisible(true);
    m_container->Focus();
}

RC<UIMenuItem> UIMenuBar::AddMenuItem(Name name, const String &text)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    RC<UIMenuItem> menu_item = GetStage()->CreateUIObject<UIMenuItem>(name, Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
    menu_item->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    menu_item->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    menu_item->SetText(text);

    // Mouse hover: set selected menu item index if this menu bar has focus
    menu_item->OnMouseHover.Bind([this, name](const MouseEvent &data) -> UIEventHandlerResult
    {
        if (HasFocus(true)) {
            const uint menu_item_index = GetMenuItemIndex(name);

            SetSelectedMenuItemIndex(menu_item_index);
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    menu_item->OnClick.Bind([this, name](const MouseEvent &data) -> UIEventHandlerResult
    {
        if (data.mouse_buttons == MouseButtonState::LEFT)
        {
            const uint menu_item_index = GetMenuItemIndex(name);

            HYP_LOG(UI, LogLevel::DEBUG, "Menu item index for item with name {}: {}", name, menu_item_index);

            if (GetSelectedMenuItemIndex() == menu_item_index) {
                SetSelectedMenuItemIndex(~0u);
            } else {
                SetSelectedMenuItemIndex(menu_item_index);
            }

            HYP_LOG(UI, LogLevel::DEBUG, "Container size {}", m_container->GetActualSize());

            return UIEventHandlerResult::STOP_BUBBLING;
        }

        return UIEventHandlerResult::OK;
    }).Detach();

    AddChildUIObject(menu_item);

    m_menu_items.PushBack(menu_item);

    UpdateMenuItemSizes();

    return menu_item;
}

RC<UIMenuItem> UIMenuBar::GetMenuItem(Name name) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    for (const RC<UIMenuItem> &menu_item : m_menu_items) {
        if (menu_item->GetName() == name) {
            return menu_item;
        }
    }

    return nullptr;
}

uint UIMenuBar::GetMenuItemIndex(Name name) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    for (SizeType i = 0; i < m_menu_items.Size(); i++) {
        if (m_menu_items[i]->GetName() == name) {
            return i;
        }
    }

    return ~0u;
}

bool UIMenuBar::RemoveMenuItem(Name name)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    const auto it = m_menu_items.FindIf([name](const RC<UIMenuItem> &menu_item)
    {
        return menu_item->GetName() == name;
    });

    if (it == m_menu_items.End()) {
        return false;
    }

    const bool removed = RemoveChildUIObject(it->Get());

    if (!removed) {
        return false;
    }

    const SizeType index = it - m_menu_items.Begin();

    m_menu_items.Erase(it);

    UpdateMenuItemSizes();

    if (m_selected_menu_item_index == index) {
        SetSelectedMenuItemIndex(m_menu_items.Any() ? m_menu_items.Size() - 1 : ~0u);
    }

    return true;
}

void UIMenuBar::UpdateMenuItemSizes()
{
    if (m_menu_items.Empty()) {
        return;
    }

    Vec2i offset = { 0, 0 };

    for (SizeType i = 0; i < m_menu_items.Size(); i++) {
        m_menu_items[i]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
        m_menu_items[i]->SetPosition(offset);

        offset.x += m_menu_items[i]->GetActualSize().x;
    }
}

#pragma region UIMenuBar

} // namespace hyperion
