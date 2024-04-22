/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIMenuBar.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>

#include <input/InputManager.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region UIMenuItem

UIMenuItem::UIMenuItem(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy))
{
    SetBorderRadius(0);
    SetPadding({ 10, 0 });
}

void UIMenuItem::Init()
{
    UIPanel::Init();

    auto text_element = m_parent->CreateUIObject<UIText>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_Text"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 14, UIObjectSize::PIXEL }));
    text_element->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    text_element->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    text_element->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    text_element->SetText(m_text);
    m_text_element = text_element;

    AddChildUIObject(text_element);

    auto drop_down_menu = m_parent->CreateUIObject<UIPanel>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_DropDownMenu"), Vec2i { 0, 0 }, UIObjectSize({ 150, UIObjectSize::PIXEL }, { 0, UIObjectSize::GROW }));
    drop_down_menu->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    drop_down_menu->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    drop_down_menu->SetBorderFlags(UI_OBJECT_BORDER_BOTTOM | UI_OBJECT_BORDER_LEFT | UI_OBJECT_BORDER_RIGHT);
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

void UIMenuItem::SetDropDownMenuItems(const Array<DropDownMenuItem> &items)
{
    m_drop_down_menu_items = items;

    UpdateDropDownMenu();
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
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
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

    if (NodeProxy node = m_drop_down_menu->GetNode()) {
        node->RemoveAllChildren();
    }

    if (m_drop_down_menu_items.Empty()) {
        return;
    }

    Vec2i offset = { 0, 0 };

    for (const DropDownMenuItem &item : m_drop_down_menu_items) {
        auto drop_down_menu_item = m_parent->CreateUIObject<UIButton>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_" + ANSIString(item.name.LookupString())), offset, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
        drop_down_menu_item->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
        drop_down_menu_item->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
        drop_down_menu_item->SetBorderFlags(UI_OBJECT_BORDER_NONE);
        drop_down_menu_item->SetBorderRadius(0);
        drop_down_menu_item->SetText(item.text);
        // drop_down_menu_item->OnClick.Bind([]); // @TODO

        drop_down_menu_item->GetTextElement()->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
        drop_down_menu_item->GetTextElement()->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);

        m_drop_down_menu->AddChildUIObject(drop_down_menu_item);

        offset += { 0, drop_down_menu_item->GetActualSize().y };
    }
}

#pragma endregion UIMenuItem

#pragma region UIMenuBar

UIMenuBar::UIMenuBar(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy)),
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
    m_container->SetBorderFlags(UI_OBJECT_BORDER_NONE);
    m_container->SetBorderRadius(0);
    m_container->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_BOTTOM_LEFT);
    m_container->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    m_container->SetPadding({ 1, 1 });
    m_container->SetDepth(100);
    m_container->SetIsVisible(false);

    m_container->OnGainFocus.Bind([this](const UIMouseEventData &data) -> bool
    {
        DebugLog(LogType::Debug, "Container gained focus!\n");

        return true;
    }).Detach();

    m_container->OnLoseFocus.Bind([this](const UIMouseEventData &data) -> bool
    {
        DebugLog(LogType::Debug, "Container lost focus!\n");

        SetSelectedMenuItemIndex(~0u);

        return true;
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

    if (NodeProxy node = m_container->GetNode()) {
        node->RemoveAllChildren();
    }

    m_container->SetIsVisible(false);

    for (uint i = 0; i < m_menu_items.Size(); i++) {
        if (i == m_selected_menu_item_index) {
            continue;
        }

        const RC<UIMenuItem> &menu_item = m_menu_items[i];

        if (!menu_item) {
            continue;
        }

        menu_item->SetFocusState(menu_item->GetFocusState() & ~UI_OBJECT_FOCUS_STATE_TOGGLED);
    }

    if (m_selected_menu_item_index >= m_menu_items.Size()) {
        m_selected_menu_item_index = ~0u;

        return;
    }

    const RC<UIMenuItem> &menu_item = m_menu_items[m_selected_menu_item_index];

    if (!menu_item || !menu_item->GetDropDownMenuElement()) {
        return;
    }

    menu_item->SetFocusState(menu_item->GetFocusState() | UI_OBJECT_FOCUS_STATE_TOGGLED);

    m_container->SetIsVisible(true);
    m_container->SetPosition({ menu_item->GetPosition().x, 0 });
    m_container->AddChildUIObject(menu_item->GetDropDownMenuElement());
    m_container->SetSize(UIObjectSize({ menu_item->GetDropDownMenuElement()->GetActualSize().x + m_container->GetPadding().x * 2, UIObjectSize::PIXEL }, { 0, UIObjectSize::GROW }));
    m_container->Focus();
}

RC<UIMenuItem> UIMenuBar::AddMenuItem(Name name, const String &text)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    auto menu_item = m_parent->CreateUIObject<UIMenuItem>(name, Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 100, UIObjectSize::PERCENT }));
    menu_item->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    menu_item->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    menu_item->SetText(text);

    // Mouse hover: set selected menu item index if this menu bar has focus
    menu_item->OnMouseHover.Bind([this, name](const UIMouseEventData &data)
    {
        if (HasFocus(true)) {
            const uint menu_item_index = GetMenuItemIndex(name);

            SetSelectedMenuItemIndex(menu_item_index);
        }

        return true;
    }).Detach();

    menu_item->OnClick.Bind([this, name](const UIMouseEventData &data)
    {
        if (data.button == MOUSE_BUTTON_LEFT)
        {
            const uint menu_item_index = GetMenuItemIndex(name);

            if (GetSelectedMenuItemIndex() == menu_item_index) {
                SetSelectedMenuItemIndex(~0u);
            } else {
                SetSelectedMenuItemIndex(menu_item_index);
            }

            return true;
        }

        return false;
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
