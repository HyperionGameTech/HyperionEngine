#include <ui/UITabView.hpp>
#include <ui/UIText.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

// UITab

UITab::UITab(ID<Entity> entity, UIScene *parent)
    : UIPanel(entity, parent)
{
    auto title_text = m_parent->CreateUIObject<UIText>(HYP_NAME(UITab_Title_Text), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::GROW }, { 100, UIObjectSize::PERCENT }));
    title_text->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    title_text->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_CENTER);
    title_text->SetText(m_title);

    UIObject::AddChildUIObject(title_text);

    m_title_text = title_text;
}

void UITab::Init()
{
    UIPanel::Init();
}

void UITab::SetTitle(const String &title)
{
    m_title = title;

    if (m_title_text != nullptr) {
        m_title_text->SetText(m_title);
    }
}

// UITabView

UITabView::UITabView(ID<Entity> entity, UIScene *parent)
    : UIPanel(entity, parent)
{
}

void UITabView::Init()
{
    UIPanel::Init();
}

RC<UITab> UITabView::AddTab(Name name, const String &title)
{
    auto tab = m_parent->CreateUIObject<UITab>(name, Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PIXEL }, { 30, UIObjectSize::PIXEL }));
    tab->SetParentAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    tab->SetOriginAlignment(UIObjectAlignment::UI_OBJECT_ALIGNMENT_TOP_LEFT);
    tab->SetTitle(title);

    UIObject::AddChildUIObject(tab.Get());

    m_tabs.PushBack(tab);

    return tab;
}

RC<UITab> UITabView::GetTab(Name name) const
{
    for (const RC<UITab> &tab : m_tabs) {
        if (tab->GetName() == name) {
            return tab;
        }
    }

    return nullptr;
}

bool UITabView::RemoveTab(Name name)
{
    const auto it = m_tabs.FindIf([name](const RC<UITab> &tab)
    {
        return tab->GetName() == name;
    });

    if (it == m_tabs.End()) {
        return false;
    }

    const bool removed = UIObject::RemoveChildUIObject(it->Get());

    if (!removed) {
        return false;
    }

    m_tabs.Erase(it);

    return true;
}

} // namespace hyperion::v2
