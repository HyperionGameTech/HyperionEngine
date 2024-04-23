/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <ui/UITabView.hpp>
#include <ui/UIText.hpp>

#include <input/InputManager.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region UITab

UITab::UITab(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy))
{
    SetBorderRadius(5);
    SetBorderFlags(UI_OBJECT_BORDER_TOP | UI_OBJECT_BORDER_LEFT | UI_OBJECT_BORDER_RIGHT);
}

void UITab::Init()
{
    UIPanel::Init();

    auto title_text = m_parent->CreateUIObject<UIText>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_Title"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 14, UIObjectSize::PIXEL }));
    title_text->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    title_text->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    title_text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    title_text->SetText(m_title);

    AddChildUIObject(title_text);

    m_title_text = title_text;

    m_contents = m_parent->CreateUIObject<UIPanel>(CreateNameFromDynamicString(ANSIString(m_name.LookupString()) + "_Contents"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    m_contents->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
}

void UITab::SetTitle(const String &title)
{
    m_title = title;

    if (m_title_text != nullptr) {
        m_title_text->SetText(m_title);
    }
}

Handle<Material> UITab::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_TAB" }) },
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

#pragma endregion UITab

#pragma region UITabView

UITabView::UITabView(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy)),
      m_selected_tab_index(~0u)
{
    SetBorderRadius(5);
    SetBorderFlags(UI_OBJECT_BORDER_BOTTOM | UI_OBJECT_BORDER_LEFT | UI_OBJECT_BORDER_RIGHT);
}

void UITabView::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    m_container = m_parent->CreateUIObject<UIPanel>(HYP_NAME(TabContents), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    m_container->SetBorderFlags(UI_OBJECT_BORDER_BOTTOM | UI_OBJECT_BORDER_LEFT | UI_OBJECT_BORDER_RIGHT);
    m_container->SetBorderRadius(5);
    m_container->SetPadding({ 5, 5 });

    AddChildUIObject(m_container);

    SetSelectedTabIndex(0);
}

void UITabView::SetSelectedTabIndex(uint index)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    if (index == m_selected_tab_index) {
        return;
    }

    m_selected_tab_index = index;

    if (NodeProxy node = m_container->GetNode()) {
        node->RemoveAllChildren();
    }

    for (uint i = 0; i < m_tabs.Size(); i++) {
        if (i == m_selected_tab_index) {
            continue;
        }

        const RC<UITab> &tab = m_tabs[i];

        if (!tab) {
            continue;
        }

        tab->SetFocusState(tab->GetFocusState() & ~UI_OBJECT_FOCUS_STATE_TOGGLED);
    }

    if (index >= m_tabs.Size()) {
        if (m_tabs.Any()) {
            m_selected_tab_index = 0;
        } else {
            m_selected_tab_index = ~0u;
        }

        return;
    }

    const RC<UITab> &tab = m_tabs[m_selected_tab_index];

    if (!tab || !tab->GetContents()) {
        return;
    }

    tab->SetFocusState(tab->GetFocusState() | UI_OBJECT_FOCUS_STATE_TOGGLED);

    m_container->AddChildUIObject(tab->GetContents());
}

RC<UITab> UITabView::AddTab(Name name, const String &title)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    auto tab = m_parent->CreateUIObject<UITab>(name, Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::PIXEL }, { 30, UIObjectSize::PIXEL }));
    tab->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    tab->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_BOTTOM_LEFT);
    tab->SetTitle(title);

    tab->OnClick.Bind([this, name](const UIMouseEventData &data) -> UIEventHandlerResult
    {
        if (data.button == MOUSE_BUTTON_LEFT)
        {
            const uint tab_index = GetTabIndex(name);
            
            DebugLog(LogType::Debug, "Tab clicked: %u\n", tab_index);

            SetSelectedTabIndex(tab_index);

            return UI_EVENT_HANDLER_RESULT_STOP_BUBBLING;
        }

        return UI_EVENT_HANDLER_RESULT_OK;
    }).Detach();

    AddChildUIObject(tab);

    m_tabs.PushBack(tab);

    UpdateTabSizes();

    if (m_selected_tab_index == ~0u) {
        SetSelectedTabIndex(0);
    }

    return tab;
}

RC<UITab> UITabView::GetTab(Name name) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    for (const RC<UITab> &tab : m_tabs) {
        if (tab->GetName() == name) {
            return tab;
        }
    }

    return nullptr;
}

uint UITabView::GetTabIndex(Name name) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    for (SizeType i = 0; i < m_tabs.Size(); i++) {
        if (m_tabs[i]->GetName() == name) {
            return i;
        }
    }

    return ~0u;
}

bool UITabView::RemoveTab(Name name)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    const auto it = m_tabs.FindIf([name](const RC<UITab> &tab)
    {
        return tab->GetName() == name;
    });

    if (it == m_tabs.End()) {
        return false;
    }

    const bool removed = RemoveChildUIObject(it->Get());

    if (!removed) {
        return false;
    }

    const SizeType index = it - m_tabs.Begin();

    m_tabs.Erase(it);

    UpdateTabSizes();

    if (m_selected_tab_index == index) {
        SetSelectedTabIndex(m_tabs.Any() ? m_tabs.Size() - 1 : ~0u);
    }

    return true;
}

void UITabView::UpdateTabSizes()
{
    if (m_tabs.Empty()) {
        return;
    }

    const Vec2i actual_size = GetActualSize();

    const int relative_tab_width = int(100.0f * (1.0f / float(m_tabs.Size())));

    for (SizeType i = 0; i < m_tabs.Size(); i++) {
        m_tabs[i]->SetSize(UIObjectSize({ relative_tab_width, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
        m_tabs[i]->SetPosition(Vec2i { int(i) * (actual_size.x / int(m_tabs.Size())), 0 });
    }
}

#pragma region UITabView

} // namespace hyperion
