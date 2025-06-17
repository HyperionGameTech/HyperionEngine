/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <ui/UITabView.hpp>
#include <ui/UIText.hpp>

#include <input/InputManager.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

#pragma region UITab

UITab::UITab()
{
    SetBorderRadius(5);
    SetBorderFlags(UIObjectBorderFlags::TOP | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    SetPadding(Vec2i { 15, 0 });
}

void UITab::Init()
{
    UIObject::Init();

    Handle<UIText> title_element = CreateUIObject<UIText>(NAME("TabTitle"), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    title_element->SetParentAlignment(UIObjectAlignment::CENTER);
    title_element->SetOriginAlignment(UIObjectAlignment::CENTER);
    title_element->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    title_element->SetText(m_text);
    title_element->SetTextSize(12.0f);

    UIObject::AddChildUIObject(title_element);

    m_title_element = title_element;

    m_contents = CreateUIObject<UIPanel>(NAME("TabContents"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    m_contents->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    m_contents->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
}

void UITab::SetText(const String& text)
{
    UIObject::SetText(text);

    if (m_title_element != nullptr)
    {
        m_title_element->SetText(m_text);
    }
}

void UITab::AddChildUIObject(const Handle<UIObject>& ui_object)
{
    if (m_contents != nullptr)
    {
        m_contents->AddChildUIObject(ui_object);

        return;
    }

    UIObject::AddChildUIObject(ui_object);
}

bool UITab::RemoveChildUIObject(UIObject* ui_object)
{
    if (m_contents != nullptr)
    {
        return m_contents->RemoveChildUIObject(ui_object);
    }

    return UIObject::RemoveChildUIObject(ui_object);
}

void UITab::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    UIObject::SetFocusState_Internal(focus_state);

    UpdateMaterial(false);
    UpdateMeshData();
}

Material::ParameterTable UITab::GetMaterialParameters() const
{
    Color color;

    if (GetFocusState() & UIObjectFocusState::TOGGLED)
    {
        color = Color(0x202124FFu);
    }
    else if (GetFocusState() & UIObjectFocusState::HOVER)
    {
        color = Color(0x3E3D40FFu);
    }
    else
    {
        color = m_background_color;
    }

    return Material::ParameterTable {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(color) }
    };
}

#pragma endregion UITab

#pragma region UITabView

UITabView::UITabView()
    : m_selected_tab_index(~0u)
{
}

void UITabView::Init()
{
    Threads::AssertOnThread(g_game_thread);

    UIPanel::Init();

    m_container = CreateUIObject<UIPanel>(NAME("TabContents"), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::FILL }));
    m_container->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    m_container->SetBorderRadius(5);
    m_container->SetPadding({ 5, 5 });
    m_container->SetBackgroundColor(Color(0x202124FFu));

    UIPanel::AddChildUIObject(m_container);

    SetSelectedTabIndex(0);
}

void UITabView::AddChildUIObject(const Handle<UIObject>& ui_object)
{
    if (!ui_object.IsValid())
    {
        return;
    }

    if (!ui_object->IsInstanceOf<UITab>())
    {
        HYP_LOG(UI, Warning, "UITabView::AddChildUIObject() called with a UIObject that is not a UITab");

        return;
    }

    auto it = m_tabs.FindAs(ui_object);

    if (it != m_tabs.End())
    {
        HYP_LOG(UI, Warning, "UITabView::AddChildUIObject() called with a UITab that is already in the tab view");

        return;
    }

    Handle<UIObject> tab = ui_object.Cast<UITab>();
    AssertThrow(tab != nullptr);

    tab->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 30, UIObjectSize::PIXEL }));

    tab->OnClick.RemoveAllDetached();
    tab->OnClick.Bind([this, name = tab->GetName()](const MouseEvent& data) -> UIEventHandlerResult
                    {
                        if (data.mouse_buttons == MouseButtonState::LEFT)
                        {
                            const uint32 tab_index = GetTabIndex(name);

                            SetSelectedTabIndex(tab_index);

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        return UIEventHandlerResult::OK;
                    })
        .Detach();

    UIPanel::AddChildUIObject(tab);

    m_tabs.PushBack(tab.Cast<UITab>());

    UpdateTabSizes();

    if (m_selected_tab_index == ~0u)
    {
        SetSelectedTabIndex(0);
    }
}

bool UITabView::RemoveChildUIObject(UIObject* ui_object)
{
    auto it = m_tabs.FindAs(ui_object);

    if (it == m_tabs.End())
    {
        return UIPanel::RemoveChildUIObject(ui_object);
    }

    return RemoveTab((*it)->GetName());
}

void UITabView::UpdateSize_Internal(bool update_children)
{
    UIPanel::UpdateSize_Internal(update_children);

    UpdateTabSizes();
}

void UITabView::SetSelectedTabIndex(uint32 index)
{
    Threads::AssertOnThread(g_game_thread);

    if (index == m_selected_tab_index)
    {
        return;
    }

    m_selected_tab_index = index;

    if (Handle<Node> node = m_container->GetNode())
    {
        node->RemoveAllChildren();
    }

    for (SizeType i = 0; i < m_tabs.Size(); i++)
    {
        if (i == m_selected_tab_index)
        {
            continue;
        }

        UITab* tab = m_tabs[i];

        if (!tab)
        {
            continue;
        }

        tab->SetFocusState(tab->GetFocusState() & ~UIObjectFocusState::TOGGLED);
    }

    if (index >= m_tabs.Size())
    {
        if (m_tabs.Any())
        {
            m_selected_tab_index = 0;
        }
        else
        {
            m_selected_tab_index = ~0u;
        }

        return;
    }

    UITab* tab = m_tabs[m_selected_tab_index];

    if (!tab || !tab->GetContents())
    {
        return;
    }

    tab->SetFocusState(tab->GetFocusState() | UIObjectFocusState::TOGGLED);

    m_container->AddChildUIObject(tab->GetContents());
}

Handle<UITab> UITabView::AddTab(Name name, const String& title)
{
    Threads::AssertOnThread(g_game_thread);

    Handle<UITab> tab = CreateUIObject<UITab>(name, Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::PIXEL }, { 30, UIObjectSize::PIXEL }));
    tab->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    tab->SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);
    tab->SetText(title);

    tab->OnClick.Bind([this, name](const MouseEvent& data) -> UIEventHandlerResult
                    {
                        if (data.mouse_buttons == MouseButtonState::LEFT)
                        {
                            const uint32 tab_index = GetTabIndex(name);

                            SetSelectedTabIndex(tab_index);

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        return UIEventHandlerResult::OK;
                    })
        .Detach();

    UIPanel::AddChildUIObject(tab);

    m_tabs.PushBack(tab);

    UpdateTabSizes();

    if (m_selected_tab_index == ~0u)
    {
        SetSelectedTabIndex(0);
    }

    return tab;
}

UITab* UITabView::GetTab(Name name) const
{
    Threads::AssertOnThread(g_game_thread);

    for (UITab* tab : m_tabs)
    {
        if (tab->GetName() == name)
        {
            return tab;
        }
    }

    return nullptr;
}

uint32 UITabView::GetTabIndex(Name name) const
{
    Threads::AssertOnThread(g_game_thread);

    for (SizeType i = 0; i < m_tabs.Size(); i++)
    {
        if (m_tabs[i]->GetName() == name)
        {
            return i;
        }
    }

    return ~0u;
}

bool UITabView::RemoveTab(Name name)
{
    Threads::AssertOnThread(g_game_thread);

    const auto it = m_tabs.FindIf([name](UITab* tab)
        {
            return tab->GetName() == name;
        });

    if (it == m_tabs.End())
    {
        return false;
    }

    const bool removed = RemoveChildUIObject(*it);

    if (!removed)
    {
        return false;
    }

    const SizeType index = it - m_tabs.Begin();

    m_tabs.Erase(it);

    UpdateTabSizes();

    if (m_selected_tab_index == index)
    {
        SetSelectedTabIndex(m_tabs.Any() ? m_tabs.Size() - 1 : ~0u);
    }

    return true;
}

void UITabView::UpdateTabSizes()
{
    if (m_tabs.Empty())
    {
        return;
    }

    const Vec2i actual_size = GetActualSize();

    int offset = 0;

    for (SizeType i = 0; i < m_tabs.Size(); i++)
    {
        m_tabs[i]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 30, UIObjectSize::PIXEL }));
        m_tabs[i]->SetPosition(Vec2i { offset, 0 });

        offset += m_tabs[i]->GetActualSize().x;
    }
}

#pragma region UITabView

} // namespace hyperion
