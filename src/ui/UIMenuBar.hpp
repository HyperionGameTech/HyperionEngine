/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_MENU_BAR_HPP
#define HYPERION_UI_MENU_BAR_HPP

#include <ui/UIPanel.hpp>

#include <core/Handle.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

class UIText;
class UIImage;
class Texture;

HYP_ENUM()
enum class UIMenuBarDropDirection : uint32
{
    DOWN, //! Default: Drop down menu opens below the menu item
    UP    //! Drop down menu opens above the menu item
};

#pragma region UIMenuItem

HYP_CLASS()
class HYP_API UIMenuItem : public UIObject
{
    HYP_OBJECT_BODY(UIMenuItem);

public:
    UIMenuItem();
    UIMenuItem(const UIMenuItem& other) = delete;
    UIMenuItem& operator=(const UIMenuItem& other) = delete;
    UIMenuItem(UIMenuItem&& other) noexcept = delete;
    UIMenuItem& operator=(UIMenuItem&& other) noexcept = delete;
    virtual ~UIMenuItem() override = default;

    /*! \brief Sets the text of the menu item.
     *
     * \param text The text of the menu item.
     */
    virtual void SetText(const String& text) override;

    void SetIconTexture(const Handle<Texture>& texture);

    /*! \brief Gets the icon element of the menu item.
     *
     *  \return The icon element of the menu item.
     */
    HYP_FORCE_INLINE const RC<UIImage>& GetIconElement() const
    {
        return m_icon_element;
    }

    /*! \brief Gets the text element of the menu item.
     *
     *  \return The text element of the menu item.
     */
    HYP_FORCE_INLINE const RC<UIText>& GetTextElement() const
    {
        return m_text_element;
    }

    /*! \brief Gets the drop down menu element.
     *
     * \return The drop down menu element.
     */
    HYP_FORCE_INLINE const RC<UIPanel>& GetDropDownMenuElement() const
    {
        return m_drop_down_menu;
    }

    HYP_FORCE_INLINE const Weak<UIMenuItem>& GetSelectedSubItem() const
    {
        return m_selected_sub_item;
    }

    void SetSelectedSubItem(const RC<UIMenuItem>& selected_sub_item);

    virtual void Init() override;

    virtual void AddChildUIObject(const RC<UIObject>& ui_object) override;
    virtual bool RemoveChildUIObject(UIObject* ui_object) override;

protected:
    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state) override;

    virtual void OnFontAtlasUpdate_Internal() override;

    virtual void SetStage_Internal(UIStage* stage) override;

    virtual Material::ParameterTable GetMaterialParameters() const override;

private:
    void UpdateDropDownMenu();
    void UpdateSubItemsDropDownMenu();

    Array<RC<UIObject>> m_menu_items;

    RC<UIText> m_text_element;
    RC<UIImage> m_icon_element;
    RC<UIPanel> m_drop_down_menu;
    RC<UIPanel> m_sub_items_drop_down_menu;
    Weak<UIMenuItem> m_selected_sub_item;
};

#pragma endregion UIMenuItem

#pragma region UIMenuBar

HYP_CLASS()
class HYP_API UIMenuBar : public UIPanel
{
    HYP_OBJECT_BODY(UIMenuBar);

public:
    UIMenuBar();
    UIMenuBar(const UIMenuBar& other) = delete;
    UIMenuBar& operator=(const UIMenuBar& other) = delete;
    UIMenuBar(UIMenuBar&& other) noexcept = delete;
    UIMenuBar& operator=(UIMenuBar&& other) noexcept = delete;
    virtual ~UIMenuBar() override = default;

    virtual void Init() override;

    HYP_METHOD(Property = "DropDirection", XMLAttribute = "direction")
    HYP_FORCE_INLINE UIMenuBarDropDirection GetDropDirection() const
    {
        return m_drop_direction;
    }

    HYP_METHOD(Property = "DropDirection", XMLAttribute = "direction")
    void SetDropDirection(UIMenuBarDropDirection drop_direction);

    /*! \brief Gets the index of the selected menu item.
     *
     * \return The index of the selected menu item.
     */
    HYP_FORCE_INLINE uint32 GetSelectedMenuItemIndex() const
    {
        return m_selected_menu_item_index;
    }

    /*! \brief Sets the selected menu item index. Set to ~0u to deselect all menu items.
     *
     * \param index The index of the menu item to select.
     */
    HYP_METHOD()
    void SetSelectedMenuItemIndex(uint32 index);

    /*! \brief Gets the menu items in the menu bar.
     *
     * \return The menu items in the menu bar.
     */
    HYP_FORCE_INLINE const Array<UIMenuItem*>& GetMenuItems() const
    {
        return m_menu_items;
    }

    /*! \brief Adds a menu item to the menu bar. Returns the menu item that was added.
     *
     * \param name The name of the menu item.
     * \param text The text of the menu item.
     * \return The menu item that was added.
     */
    HYP_METHOD()
    RC<UIMenuItem> AddMenuItem(Name name, const String& text);

    /*! \brief Gets a menu item by name. Returns nullptr if the menu item was not found.
     *
     * \param name The name of the menu item to get.
     * \return The menu item, or nullptr if the menu item was not found.
     */
    HYP_METHOD()
    UIMenuItem* GetMenuItem(Name name) const;

    /*! \brief Gets the index of a menu item by name. Returns ~0u if the menu item was not found.
     *
     * \param name The name of the menu item to get the index of.
     * \return The index of the menu item, or ~0u if the menu item was not found.
     */
    HYP_METHOD()
    uint32 GetMenuItemIndex(Name name) const;

    /*! \brief Removes a menu item by name.
     *
     * \param name The name of the menu item to remove.
     * \return True if the menu item was removed, false otherwise.
     */
    HYP_METHOD()
    bool RemoveMenuItem(Name name);

    // overloads to allow adding a UIMenuItem
    virtual void AddChildUIObject(const RC<UIObject>& ui_object) override;
    virtual bool RemoveChildUIObject(UIObject* ui_object) override;

protected:
    virtual void UpdateSize_Internal(bool update_children) override;

    virtual void SetStage_Internal(UIStage* stage) override;

    virtual void OnRemoved_Internal() override;

private:
    void UpdateMenuItemSizes();

    Vec2i GetDropDownMenuPosition(UIMenuItem* menu_item) const;

    UIMenuBarDropDirection m_drop_direction;

    Array<UIMenuItem*> m_menu_items;

    RC<UIPanel> m_container;

    uint32 m_selected_menu_item_index;
};

#pragma endregion UIMenuBar

} // namespace hyperion

#endif