/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <UI/UIPanel.hpp>

#include <Core/Containers/Array.hpp>

namespace Hyperion {

class UIText;

#pragma region UITab

HYP_CLASS()
class ENGINE_API UITab : public UIObject
{
    HYP_OBJECT_BODY(UITab);

public:
    UITab();
    UITab(const UITab& other) = delete;
    UITab& operator=(const UITab& other) = delete;
    UITab(UITab&& other) noexcept = delete;
    UITab& operator=(UITab&& other) noexcept = delete;
    virtual ~UITab() override = default;

    virtual void SetText(const String& text) override;

    HYP_FORCE_INLINE const Handle<UIPanel>& GetContents() const
    {
        return m_contents;
    }

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

protected:
    virtual void Init() override;

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState) override;

    virtual MaterialParameters GetMaterialParameters() const override;

private:
    Handle<UIText> m_titleElement;
    Handle<UIPanel> m_contents;
};

#pragma endregion UITab

#pragma region UITabView

HYP_CLASS()
class ENGINE_API UITabView : public UIPanel
{
    HYP_OBJECT_BODY(UITabView);

public:
    UITabView();
    UITabView(const UITabView& other) = delete;
    UITabView& operator=(const UITabView& other) = delete;
    UITabView(UITabView&& other) noexcept = delete;
    UITabView& operator=(UITabView&& other) noexcept = delete;
    virtual ~UITabView() override = default;

    /*! \brief Gets the index of the selected tab.
     *
     * \return The index of the selected tab.
     */
    HYP_FORCE_INLINE uint32 GetSelectedTabIndex() const
    {
        return m_selectedTabIndex;
    }

    /*! \brief Sets the selected tab by index.
     *
     * \param index The index of the tab.
     */
    void SetSelectedTabIndex(uint32 index);

    HYP_FORCE_INLINE const Array<UITab*>& GetTabs() const
    {
        return m_tabs;
    }

    /*! \brief Add a tab to the tab view.
     *
     * \param name The name of the tab.
     * \param title The title of the tab.
     * \return A reference counted pointer to the tab.
     */
    Handle<UITab> AddTab(Name name, const String& title);

    /*! \brief Get a tab by name. Returns nullptr if the tab does not exist.
     *
     * \param name The name of the tab.
     * \return A reference counted pointer to tab, or nullptr if the tab does not exist.
     */
    UITab* GetTab(Name name) const;

    /*! \brief Get the tab index by name. Returns ~0u if the tab does not exist.
     *
     * \param name The name of the tab.
     * \return The index of the tab.
     */
    uint32 GetTabIndex(Name name) const;

    /*! \brief Remove a tab by name.
     *
     * \param name The name of the tab.
     * \return True if the tab was removed, false if the tab does not exist.
     */
    bool RemoveTab(Name name);

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

protected:
    virtual void Init() override;

private:
    virtual void UpdateSize_Internal(bool updateChildren) override;

    void UpdateTabSizes();

    Handle<UIPanel> m_container;

    Array<UITab*> m_tabs;

    uint32 m_selectedTabIndex;
};

#pragma endregion UITabView

} // namespace Hyperion
