/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <ui/UIPanel.hpp>

#include <core/object/Handle.hpp>

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
    HYP_FORCE_INLINE const Handle<UIImage>& GetIconElement() const
    {
        return m_iconElement;
    }

    /*! \brief Gets the text element of the menu item.
     *
     *  \return The text element of the menu item.
     */
    HYP_FORCE_INLINE const Handle<UIText>& GetTextElement() const
    {
        return m_textElement;
    }

    /*! \brief Gets the drop down menu element.
     *
     * \return The drop down menu element.
     */
    HYP_FORCE_INLINE const Handle<UIPanel>& GetDropDownMenuElement() const
    {
        return m_dropDownMenu;
    }

    HYP_FORCE_INLINE const WeakHandle<UIMenuItem>& GetSelectedSubItem() const
    {
        return m_selectedSubItem;
    }

    void SetSelectedSubItem(const Handle<UIMenuItem>& selectedSubItem);

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

protected:
    virtual void Init() override;

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState) override;

    virtual void OnFontAtlasUpdate_Internal() override;

    virtual void SetStage_Internal(UIStage* stage) override;

    virtual Material::ParameterTable GetMaterialParameters() const override;

private:
    void UpdateDropDownMenu();
    void UpdateSubItemsDropDownMenu();

    Array<Handle<UIObject>> m_menuItems;

    Handle<UIText> m_textElement;
    Handle<UIImage> m_iconElement;
    Handle<UIPanel> m_dropDownMenu;
    Handle<UIPanel> m_subItemsDropDownMenu;
    WeakHandle<UIMenuItem> m_selectedSubItem;
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

    HYP_METHOD(Property = "DropDirection", XMLAttribute = "direction")
    HYP_FORCE_INLINE UIMenuBarDropDirection GetDropDirection() const
    {
        return m_dropDirection;
    }

    HYP_METHOD(Property = "DropDirection", XMLAttribute = "direction")
    void SetDropDirection(UIMenuBarDropDirection dropDirection);

    /*! \brief Gets the index of the selected menu item.
     *
     * \return The index of the selected menu item.
     */
    HYP_FORCE_INLINE uint32 GetSelectedMenuItemIndex() const
    {
        return m_selectedMenuItemIndex;
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
        return m_menuItems;
    }

    /*! \brief Adds a menu item to the menu bar. Returns the menu item that was added.
     *
     * \param name The name of the menu item.
     * \param text The text of the menu item.
     * \return The menu item that was added.
     */
    HYP_METHOD()
    Handle<UIMenuItem> AddMenuItem(Name name, const String& text);

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

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

protected:
    virtual void Init() override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    virtual void SetStage_Internal(UIStage* stage) override;

    virtual void OnRemoved_Internal() override;

private:
    void UpdateMenuItemSizes();

    Vec2i GetDropDownMenuPosition(UIMenuItem* menuItem) const;

    UIMenuBarDropDirection m_dropDirection;

    Array<UIMenuItem*> m_menuItems;

    Handle<UIPanel> m_container;

    uint32 m_selectedMenuItemIndex;
};

#pragma endregion UIMenuBar

} // namespace hyperion

