/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_MENU_BAR_HPP
#define HYPERION_UI_MENU_BAR_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

class UIText;

#pragma region DropDownMenuItem

struct DropDownMenuItem
{
    Name        name;
    String      text;
    Proc<void>  action;
};

#pragma endregion DropDownMenuItem

#pragma region UIMenuItem

class HYP_API UIMenuItem : public UIPanel
{
public:
    UIMenuItem(UIStage *stage, NodeProxy node_proxy);
    UIMenuItem(const UIMenuItem &other)                 = delete;
    UIMenuItem &operator=(const UIMenuItem &other)      = delete;
    UIMenuItem(UIMenuItem &&other) noexcept             = delete;
    UIMenuItem &operator=(UIMenuItem &&other) noexcept  = delete;
    virtual ~UIMenuItem() override                      = default;

    /*! \brief Gets the text of the menu item.
     * 
     * \return The text of the menu item.
     */
    HYP_NODISCARD HYP_FORCE_INLINE
    const String &GetText() const
        { return m_text; }

    /*! \brief Sets the text of the menu item.
     * 
     * \param text The text of the menu item.
     */
    void SetText(const String &text);

    /*! \brief Adds a DropDownMenuItem to the menu item.
     *
     * \param item The DropDownMenuItem to add.
     */
    void AddDropDownMenuItem(DropDownMenuItem &&item);

    /*! \brief Gets the list of DropDownMenuItems.
     * 
     * \return The array of DropDownMenuItems.
     */
    HYP_NODISCARD HYP_FORCE_INLINE
    const Array<DropDownMenuItem> &GetDropDownMenuItems() const
        { return m_drop_down_menu_items; }

    /*! \brief Sets the list of DropDownMenuItems.
     * 
     * \param items The array of DropDownMenuItems.
     */
    void SetDropDownMenuItems(Array<DropDownMenuItem> items);

    /*! \brief Get a dropdown menu item by name.
     *
     * \param name The name of the dropdown menu item.
     * \return The dropdown menu item.
     */
    HYP_NODISCARD 
    DropDownMenuItem *GetDropDownMenuItem(Name name);

    /*! \brief Get a dropdown menu item by name.
     *
     * \param name The name of the dropdown menu item.
     * \return The dropdown menu item.
     */
    HYP_NODISCARD 
    const DropDownMenuItem *GetDropDownMenuItem(Name name) const;

    /*! \brief Gets the drop down menu element.
     * 
     * \return The drop down menu element.
     */
    HYP_NODISCARD HYP_FORCE_INLINE
    const RC<UIPanel> &GetDropDownMenuElement() const
        { return m_drop_down_menu; }

    HYP_NODISCARD
    virtual bool IsContainer() const override
        { return false; }

    virtual void Init() override;

protected:
    virtual Handle<Material> GetMaterial() const override;

private:
    void UpdateDropDownMenu();

    String                  m_text;
    Array<DropDownMenuItem> m_drop_down_menu_items;

    RC<UIText>              m_text_element;
    RC<UIPanel>             m_drop_down_menu;
};

#pragma endregion UIMenuItem

#pragma region UIMenuBar

class HYP_API UIMenuBar : public UIPanel
{
public:
    UIMenuBar(UIStage *stage, NodeProxy node_proxy);
    UIMenuBar(const UIMenuBar &other)                   = delete;
    UIMenuBar &operator=(const UIMenuBar &other)        = delete;
    UIMenuBar(UIMenuBar &&other) noexcept               = delete;
    UIMenuBar &operator=(UIMenuBar &&other) noexcept    = delete;
    virtual ~UIMenuBar() override                       = default;

    virtual void Init() override;

    /*! \brief Gets the index of the selected menu item.
     * 
     * \return The index of the selected menu item.
     */
    HYP_NODISCARD HYP_FORCE_INLINE
    uint GetSelectedMenuItemIndex() const
        { return m_selected_menu_item_index; }

    /*! \brief Sets the selected menu item index. Set to ~0u to deselect all menu items.
     * 
     * \param index The index of the menu item to select.
     */
    void SetSelectedMenuItemIndex(uint index);

    /*! \brief Gets the menu items in the menu bar.
     * 
     * \return The menu items in the menu bar.
     */
    HYP_NODISCARD HYP_FORCE_INLINE
    const Array<RC<UIMenuItem>> &GetMenuItems() const
        { return m_menu_items; }

    /*! \brief Adds a menu item to the menu bar. Returns the menu item that was added.
     * 
     * \param name The name of the menu item.
     * \param text The text of the menu item.
     * \return The menu item that was added.
     */
    RC<UIMenuItem> AddMenuItem(Name name, const String &text);

    /*! \brief Gets a menu item by name. Returns nullptr if the menu item was not found.
     * 
     * \param name The name of the menu item to get.
     * \return The menu item, or nullptr if the menu item was not found.
     */
    HYP_NODISCARD 
    RC<UIMenuItem> GetMenuItem(Name name) const;

    /*! \brief Gets the index of a menu item by name. Returns ~0u if the menu item was not found.
     * 
     * \param name The name of the menu item to get the index of.
     * \return The index of the menu item, or ~0u if the menu item was not found.
     */
    HYP_NODISCARD 
    uint GetMenuItemIndex(Name name) const;

    /*! \brief Removes a menu item by name.
     * 
     * \param name The name of the menu item to remove.
     * \return True if the menu item was removed, false otherwise.
     */
    bool RemoveMenuItem(Name name);

protected:
    virtual void OnRemoved_Internal() override;

private:
    void UpdateMenuItemSizes();

    Array<RC<UIMenuItem>>   m_menu_items;

    RC<UIPanel>             m_container;

    uint                    m_selected_menu_item_index;
};

#pragma endregion UIMenuBar

} // namespace hyperion

#endif