/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIMenuBar.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>

#include <input/InputManager.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region UIMenuItem

UIMenuItem::UIMenuItem(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UOT_MENU_ITEM)
{
    SetBorderRadius(0);
    SetPadding({ 10, 0 });
}

void UIMenuItem::Init()
{
    UIPanel::Init();

    auto text_element = m_parent->CreateUIObject<UIText>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 14, UIObjectSize::PIXEL }));
    text_element->SetParentAlignment(UIObjectAlignment::UOA_CENTER);
    text_element->SetOriginAlignment(UIObjectAlignment::UOA_CENTER);
    text_element->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    text_element->SetText(m_text);
    m_text_element = text_element;

    AddChildUIObject(text_element);

    auto drop_down_menu = m_parent->CreateUIObject<UIPanel>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_DropDownMenu"), Vec2i { 0, 0 }, UIObjectSize({ 150, UIObjectSize::PIXEL }, { 0, UIObjectSize::GROW }));
    drop_down_menu->SetAcceptsFocus(false);
    drop_down_menu->SetParentAlignment(UIObjectAlignment::UOA_TOP_LEFT);
    drop_down_menu->SetOriginAlignment(UIObjectAlignment::UOA_TOP_LEFT);
    drop_down_menu->SetBorderFlags(UOB_BOTTOM | UOB_LEFT | UOB_RIGHT);
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
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_BUTTON" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributes::RAF_NONE,
            .layer              = GetDrawableLayer()
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.05f, 0.06f, 0.09f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture> { } }
        }
    );
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

        auto drop_down_menu_item = m_drop_down_menu->FindChildUIObject(drop_down_menu_item_name).Cast<UIButton>();

        if (drop_down_menu_item != nullptr) {
            offset += { 0, drop_down_menu_item->GetActualSize().y };

            continue;
        }

        drop_down_menu_item = m_parent->CreateUIObject<UIButton>(drop_down_menu_item_name, offset, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
        drop_down_menu_item->SetAcceptsFocus(false);
        drop_down_menu_item->SetParentAlignment(UIObjectAlignment::UOA_TOP_LEFT);
        drop_down_menu_item->SetOriginAlignment(UIObjectAlignment::UOA_TOP_LEFT);
        drop_down_menu_item->SetBorderFlags(UOB_NONE);
        drop_down_menu_item->SetBorderRadius(0);
        drop_down_menu_item->SetText(item.text);

        drop_down_menu_item->OnClick.Bind([this, name = item.name](const UIMouseEventData &data) -> UIEventHandlerResult
        {
            DebugLog(LogType::Debug, "OnClick item with name %s\n", *name);

            DropDownMenuItem *item_ptr = GetDropDownMenuItem(name);

            if (item_ptr == nullptr) {
                DebugLog(LogType::Warn, "Could not find drop down menu item with name %s\n", *name);

                return UEHR_ERR;
            }

            if (!item_ptr->action.IsValid()) {
                return UEHR_OK;
            }

            item_ptr->action();

            return UEHR_OK; // bubble up to container so it can close
        }).Detach();

        drop_down_menu_item->GetTextElement()->SetParentAlignment(UIObjectAlignment::UOA_TOP_LEFT);
        drop_down_menu_item->GetTextElement()->SetOriginAlignment(UIObjectAlignment::UOA_TOP_LEFT);

        m_drop_down_menu->AddChildUIObject(drop_down_menu_item);

        offset += { 0, drop_down_menu_item->GetActualSize().y };
    }
}

#pragma endregion UIMenuItem

#pragma region UIMenuBar

UIMenuBar::UIMenuBar(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UOT_MENU_BAR),
      m_selected_menu_item_index(~0u)
{
    SetBorderRadius(0);
    SetPadding({ 5, 2 });
}

void UIMenuBar::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    m_container = m_parent->CreateUIObject<UIPanel>(HYP_NAME(MenuItemContents), Vec2i { 0, 0 }, UIObjectSize({ 250, UIObjectSize::PIXEL }, { 0, UIObjectSize::GROW }));
    m_container->SetIsVisible(false);
    m_container->SetBorderFlags(UOB_NONE);
    m_container->SetBorderRadius(0);
    m_container->SetParentAlignment(UIObjectAlignment::UOA_BOTTOM_LEFT);
    m_container->SetOriginAlignment(UIObjectAlignment::UOA_TOP_LEFT);
    m_container->SetPadding({ 1, 1 });
    m_container->SetDepth(100);

    m_container->OnClick.Bind([this](const UIMouseEventData &data) -> UIEventHandlerResult
    {
        // Hide container on any item clicked
        SetSelectedMenuItemIndex(~0u);
        // Lose focus of the container (otherwise hovering over other menu items will cause the menu strips to reappear)
        Blur();

        return UEHR_STOP_BUBBLING;
    }).Detach();

    m_container->OnLoseFocus.Bind([this](const UIMouseEventData &data) -> UIEventHandlerResult
    {
        // First check that any child items aren't focused before hiding the container.
        if (!HasFocus(true)) {
            SetSelectedMenuItemIndex(~0u);
        }

        return UEHR_OK;
    }).Detach();

    AddChildUIObject(m_container);
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

        menu_item->SetFocusState(menu_item->GetFocusState() & ~UOFS_TOGGLED);
    }

    if (m_selected_menu_item_index >= m_menu_items.Size()) {
        m_selected_menu_item_index = ~0u;

        return;
    }

    const RC<UIMenuItem> &menu_item = m_menu_items[m_selected_menu_item_index];

    if (!menu_item || !menu_item->GetDropDownMenuElement()) {
        return;
    }

    menu_item->SetFocusState(menu_item->GetFocusState() | UOFS_TOGGLED);

    m_container->SetPosition({ menu_item->GetPosition().x, 0 });
    m_container->AddChildUIObject(menu_item->GetDropDownMenuElement());
    m_container->SetSize(UIObjectSize({ menu_item->GetDropDownMenuElement()->GetActualSize().x + m_container->GetPadding().x * 2, UIObjectSize::PIXEL }, { 0, UIObjectSize::GROW }));
    m_container->Focus();
    m_container->SetIsVisible(true);
}

RC<UIMenuItem> UIMenuBar::AddMenuItem(Name name, const String &text)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    auto menu_item = m_parent->CreateUIObject<UIMenuItem>(name, Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 100, UIObjectSize::PERCENT }));
    menu_item->SetParentAlignment(UIObjectAlignment::UOA_TOP_LEFT);
    menu_item->SetOriginAlignment(UIObjectAlignment::UOA_TOP_LEFT);
    menu_item->SetText(text);

    // Mouse hover: set selected menu item index if this menu bar has focus
    menu_item->OnMouseHover.Bind([this, name](const UIMouseEventData &data) -> UIEventHandlerResult
    {
        if (HasFocus(true)) {
            const uint menu_item_index = GetMenuItemIndex(name);

            SetSelectedMenuItemIndex(menu_item_index);
        }

        return UEHR_OK;
    }).Detach();

    menu_item->OnClick.Bind([this, name](const UIMouseEventData &data) -> UIEventHandlerResult
    {
        if (data.button == MouseButton::MB_LEFT)
        {
            const uint menu_item_index = GetMenuItemIndex(name);

            if (GetSelectedMenuItemIndex() == menu_item_index) {
                SetSelectedMenuItemIndex(~0u);
            } else {
                SetSelectedMenuItemIndex(menu_item_index);
            }

            return UEHR_STOP_BUBBLING;
        }

        return UEHR_OK;
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
        m_menu_items[i]->SetSize(UIObjectSize({ 0, UIObjectSize::GROW }, { 100, UIObjectSize::PERCENT }));
        m_menu_items[i]->SetPosition(offset);

        offset.x += m_menu_items[i]->GetActualSize().x;
    }
}

#pragma region UIMenuBar

} // namespace hyperion
