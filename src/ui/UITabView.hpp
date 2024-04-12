#ifndef HYPERION_V2_UI_TAB_VIEW_HPP
#define HYPERION_V2_UI_TAB_VIEW_HPP

#include <ui/UIPanel.hpp>

#include <core/lib/DynArray.hpp>

namespace hyperion::v2 {

class UIText;

class HYP_API UITab : public UIPanel
{
public:
    UITab(ID<Entity> entity, UIScene *ui_scene);
    UITab(const UITab &other)                   = delete;
    UITab &operator=(const UITab &other)        = delete;
    UITab(UITab &&other) noexcept               = delete;
    UITab &operator=(UITab &&other) noexcept    = delete;
    virtual ~UITab() override                   = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const String &GetTitle() const
        { return m_title; }

    void SetTitle(const String &title);

    virtual void Init() override;

protected:
    virtual Handle<Material> GetMaterial() const override;

private:
    String      m_title;
    RC<UIText>  m_title_text;
};

// UITabView

class HYP_API UITabView : public UIPanel
{
public:
    UITabView(ID<Entity> entity, UIScene *ui_scene);
    UITabView(const UITabView &other)                   = delete;
    UITabView &operator=(const UITabView &other)        = delete;
    UITabView(UITabView &&other) noexcept               = delete;
    UITabView &operator=(UITabView &&other) noexcept    = delete;
    virtual ~UITabView() override                       = default;

    virtual void Init() override;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Array<RC<UITab>> &GetTabs() const
        { return m_tabs; }

    RC<UITab> AddTab(Name name, const String &title);
    RC<UITab> GetTab(Name name) const;
    bool RemoveTab(Name name);

private:
    Array<RC<UITab>>    m_tabs;
};

} // namespace hyperion::v2

#endif