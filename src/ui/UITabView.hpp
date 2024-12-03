/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_TAB_VIEW_HPP
#define HYPERION_UI_TAB_VIEW_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

class UIText;

#pragma region UITab

HYP_CLASS()
class HYP_API UITab : public UIObject
{
    HYP_OBJECT_BODY(UITab);

public:
    UITab();
    UITab(const UITab &other)                   = delete;
    UITab &operator=(const UITab &other)        = delete;
    UITab(UITab &&other) noexcept               = delete;
    UITab &operator=(UITab &&other) noexcept    = delete;
    virtual ~UITab() override                   = default;

    virtual void SetText(const String &text) override;

    HYP_FORCE_INLINE const RC<UIPanel> &GetContents() const
        { return m_contents; }

    virtual void Init() override;

    virtual void AddChildUIObject(const RC<UIObject> &ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

protected:
    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state) override;

    virtual Material::ParameterTable GetMaterialParameters() const override;

private:
    RC<UIText>  m_title_element;
    RC<UIPanel> m_contents;
};

#pragma endregion UITab

#pragma region UITabView

HYP_CLASS()
class HYP_API UITabView : public UIPanel
{
    HYP_OBJECT_BODY(UITabView);

public:
    UITabView();
    UITabView(const UITabView &other)                   = delete;
    UITabView &operator=(const UITabView &other)        = delete;
    UITabView(UITabView &&other) noexcept               = delete;
    UITabView &operator=(UITabView &&other) noexcept    = delete;
    virtual ~UITabView() override                       = default;

    virtual void Init() override;

    /*! \brief Gets the index of the selected tab.
     * 
     * \return The index of the selected tab.
     */
    HYP_FORCE_INLINE uint GetSelectedTabIndex() const
        { return m_selected_tab_index; }

    /*! \brief Sets the selected tab by index.
     * 
     * \param index The index of the tab.
     */
    void SetSelectedTabIndex(uint index);

    HYP_FORCE_INLINE const Array<RC<UITab>> &GetTabs() const
        { return m_tabs; }

    /*! \brief Add a tab to the tab view.
     * 
     * \param name The name of the tab.
     * \param title The title of the tab.
     * \return A reference counted pointer to the tab.
     */
    RC<UITab> AddTab(Name name, const String &title);

    /*! \brief Get a tab by name. Returns nullptr if the tab does not exist.
     * 
     * \param name The name of the tab.
     * \return A reference counted pointer to tab, or nullptr if the tab does not exist.
     */
    RC<UITab> GetTab(Name name) const;

    /*! \brief Get the tab index by name. Returns ~0u if the tab does not exist.
     * 
     * \param name The name of the tab.
     * \return The index of the tab.
     */
    uint GetTabIndex(Name name) const;

    /*! \brief Remove a tab by name.
     * 
     * \param name The name of the tab.
     * \return True if the tab was removed, false if the tab does not exist.
     */
    bool RemoveTab(Name name);

    virtual void AddChildUIObject(const RC<UIObject> &ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

private:
    virtual void UpdateSize_Internal(bool update_children) override;

    void UpdateTabSizes();

    RC<UIPanel>         m_container;

    Array<RC<UITab>>    m_tabs;

    uint                m_selected_tab_index;
};

#pragma endregion UITabView

} // namespace hyperion

#endif