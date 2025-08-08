/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIMenuBar.hpp>
#include <ui/UISpacer.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIImage.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>

#include <core/object/HypClass.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIMenuItem

UIMenuItem::UIMenuItem()
{
    SetBorderRadius(0);
    SetPadding({ 5, 2 });
    SetBackgroundColor(Color::Transparent());

    OnEnabled
        .Bind([this]()
            {
                UpdateMaterial(false);

                return UIEventHandlerResult::OK;
            })
        .Detach();

    OnDisabled
        .Bind([this]()
            {
                UpdateMaterial(false);

                return UIEventHandlerResult::OK;
            })
        .Detach();
}

void UIMenuItem::Init()
{
    UIObject::Init();

    Handle<UIMenuBar> menuBar = GetClosestSpawnParent<UIMenuBar>();
    Assert(menuBar != nullptr);

    Handle<UIImage> iconElement = CreateUIObject<UIImage>(CreateNameFromDynamicString(HYP_FORMAT("{}_Icon", GetName())), Vec2i { 0, 0 }, UIObjectSize({ 16, UIObjectSize::PIXEL }, { 16, UIObjectSize::PIXEL }));
    iconElement->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    iconElement->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    iconElement->SetIsVisible(false);
    m_iconElement = iconElement;

    UIObject::AddChildUIObject(m_iconElement);

    Handle<UIText> textElement = CreateUIObject<UIText>(CreateNameFromDynamicString(HYP_FORMAT("{}_Text", GetName())), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    textElement->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    textElement->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    textElement->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    textElement->SetText(m_text);
    m_textElement = textElement;

    UIObject::AddChildUIObject(m_textElement);

    Handle<UIPanel> dropDownMenu = CreateUIObject<UIPanel>(CreateNameFromDynamicString(HYP_FORMAT("{}_DropDown", GetName())), Vec2i { 0, 0 }, UIObjectSize({ 150, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));
    dropDownMenu->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    dropDownMenu->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    dropDownMenu->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    dropDownMenu->SetBackgroundColor(Vec4f { 0.0f, 0.0f, 0.0f, 0.0f });
    m_dropDownMenu = dropDownMenu;
}

void UIMenuItem::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    if (!uiObject)
    {
        return;
    }

    auto it = m_menuItems.Find(uiObject.Get());

    if (it != m_menuItems.End())
    {
        // already exists
        return;
    }

    m_menuItems.PushBack(uiObject);

    UpdateDropDownMenu();

    if (!uiObject.IsValid())
    {
        return;
    }

    if (Handle<UIMenuItem> menuItem = ObjCast<UIMenuItem>(uiObject))
    {
        menuItem->OnMouseHover
            .Bind([weakThis = WeakHandleFromThis(), subMenuItemWeak = menuItem.ToWeak()](const MouseEvent& event) -> UIEventHandlerResult
                {
                    Handle<UIMenuItem> menuItem = weakThis.Lock();
                    Handle<UIMenuItem> subMenuItem = subMenuItemWeak.Lock();

                    if (!menuItem || !subMenuItem)
                    {
                        menuItem->SetSelectedSubItem(nullptr);

                        return UIEventHandlerResult::OK;
                    }

                    if (!subMenuItem->GetDropDownMenuElement() || !subMenuItem->GetDropDownMenuElement()->HasChildUIObjects())
                    {
                        menuItem->SetSelectedSubItem(nullptr);

                        return UIEventHandlerResult::OK;
                    }

                    menuItem->SetSelectedSubItem(subMenuItem);

                    return UIEventHandlerResult::STOP_BUBBLING;
                })
            .Detach();
    }
}

bool UIMenuItem::RemoveChildUIObject(UIObject* uiObject)
{
    if (!uiObject)
    {
        return false;
    }

    if (uiObject->IsA<UIMenuItem>())
    {
        auto it = m_menuItems.FindAs(uiObject);

        if (it != m_menuItems.End())
        {
            m_menuItems.Erase(it);
        }

        UpdateDropDownMenu();

        return true;
    }

    return m_dropDownMenu->RemoveChildUIObject(uiObject);
}

void UIMenuItem::SetIconTexture(const Handle<Texture>& texture)
{
    m_iconElement->SetTexture(texture);

    if (texture.IsValid())
    {
        m_iconElement->SetIsVisible(true);

        m_textElement->SetPosition(Vec2i { m_iconElement->GetActualSize().x + 5, 0 });
    }
    else
    {
        m_iconElement->SetIsVisible(false);

        m_textElement->SetPosition(Vec2i { 0, 0 });
    }
}

void UIMenuItem::SetText(const String& text)
{
    UIObject::SetText(text);

    if (m_textElement != nullptr)
    {
        m_textElement->SetText(m_text);
    }

    UpdateSize();
}

void UIMenuItem::UpdateDropDownMenu()
{
    Assert(m_dropDownMenu != nullptr);

    m_dropDownMenu->RemoveAllChildUIObjects();

    if (m_menuItems.Empty())
    {
        return;
    }

    Vec2i offset = { 0, 0 };

    for (const Handle<UIObject>& menuItem : m_menuItems)
    {
        menuItem->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        menuItem->SetPosition(offset);

        m_dropDownMenu->AddChildUIObject(menuItem);

        offset += { 0, menuItem->GetActualSize().y };
    }

    m_dropDownMenu->UpdateSize();
}

void UIMenuItem::UpdateSubItemsDropDownMenu()
{
    Handle<UIMenuItem> selectedSubItem = m_selectedSubItem.Lock();

    if (!selectedSubItem.IsValid())
    {
        if (m_subItemsDropDownMenu != nullptr)
        {
            m_subItemsDropDownMenu->RemoveFromParent();
            m_subItemsDropDownMenu = nullptr;
        }

        return;
    }

    if (m_subItemsDropDownMenu == nullptr)
    {
        m_subItemsDropDownMenu = CreateUIObject<UIPanel>(CreateNameFromDynamicString(HYP_FORMAT("{}_SubItemsDropDown", GetName())), Vec2i { 0, 0 }, UIObjectSize({ 150, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));
        m_subItemsDropDownMenu->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
        m_subItemsDropDownMenu->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
        m_subItemsDropDownMenu->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    }

    m_subItemsDropDownMenu->RemoveAllChildUIObjects();

    if (!selectedSubItem->GetDropDownMenuElement())
    {
        return;
    }

    m_subItemsDropDownMenu->RemoveFromParent();

    m_subItemsDropDownMenu->AddChildUIObject(selectedSubItem->GetDropDownMenuElement());
    m_subItemsDropDownMenu->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 300, UIObjectSize::PIXEL }));
    m_subItemsDropDownMenu->SetPosition(Vec2i { int(selectedSubItem->GetAbsolutePosition().x) + m_dropDownMenu->GetActualSize().x, int(selectedSubItem->GetAbsolutePosition().y) });
    m_subItemsDropDownMenu->SetIsVisible(true);
    m_subItemsDropDownMenu->SetDepth(100);
    m_subItemsDropDownMenu->Focus();

    m_subItemsDropDownMenu->OnClick.RemoveAllDetached();
    m_subItemsDropDownMenu->OnClick
        .Bind([weakThis = WeakHandleFromThis()](const MouseEvent& data) -> UIEventHandlerResult
            {
                Handle<UIMenuItem> menuItem = weakThis.Lock();

                if (!menuItem)
                {
                    return UIEventHandlerResult::OK;
                }

                Handle<UIMenuBar> menuBar = menuItem->GetClosestSpawnParent<UIMenuBar>();

                if (!menuBar)
                {
                    return UIEventHandlerResult::OK;
                }

                menuBar->SetSelectedMenuItemIndex(~0u);

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    if (m_stage != nullptr)
    {
        m_stage->AddChildUIObject(m_subItemsDropDownMenu);
    }
}

void UIMenuItem::SetSelectedSubItem(const Handle<UIMenuItem>& selectedSubItem)
{
    HYP_SCOPE;

    if (m_selectedSubItem == selectedSubItem)
    {
        return;
    }

    m_selectedSubItem = selectedSubItem;

    UpdateSubItemsDropDownMenu();
}

void UIMenuItem::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState)
{
    const EnumFlags<UIObjectFocusState> previousFocusState = GetFocusState();

    UIObject::SetFocusState_Internal(focusState);

    if ((previousFocusState & (UIObjectFocusState::HOVER | UIObjectFocusState::TOGGLED | UIObjectFocusState::PRESSED)) != (focusState & (UIObjectFocusState::HOVER | UIObjectFocusState::TOGGLED | UIObjectFocusState::PRESSED)))
    {
        if (!(GetFocusState() & UIObjectFocusState::TOGGLED))
        {
            SetSelectedSubItem(nullptr);
        }

        UpdateMaterial(false);
    }
}

void UIMenuItem::OnFontAtlasUpdate_Internal()
{
    UpdateDropDownMenu();
}

void UIMenuItem::SetStage_Internal(UIStage* stage)
{
    UIObject::SetStage_Internal(stage);

    if (m_subItemsDropDownMenu != nullptr)
    {
        m_subItemsDropDownMenu->RemoveFromParent();

        if (m_stage != nullptr)
        {
            m_stage->AddChildUIObject(m_subItemsDropDownMenu);
        }
    }
}

Material::ParameterTable UIMenuItem::GetMaterialParameters() const
{
    Color color = GetBackgroundColor();

    if (IsEnabled())
    {
        const EnumFlags<UIObjectFocusState> focusState = GetFocusState();

        if (focusState & (UIObjectFocusState::TOGGLED | UIObjectFocusState::PRESSED))
        {
            color = Color(Vec4f { 0.5f, 0.5f, 0.5f, 1.0f });
        }
        else if (focusState & UIObjectFocusState::HOVER)
        {
            color = Color(Vec4f { 0.3f, 0.3f, 0.3f, 1.0f });
        }
    }

    return Material::ParameterTable {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(color) }
    };
}

#pragma endregion UIMenuItem

#pragma region UIMenuBar

UIMenuBar::UIMenuBar()
    : m_dropDirection(UIMenuBarDropDirection::DOWN),
      m_selectedMenuItemIndex(~0u)
{
    SetBorderRadius(0);
    SetPadding({ 5, 2 });
}

void UIMenuBar::Init()
{
    Threads::AssertOnThread(g_gameThread);

    UIPanel::Init();

    m_container = CreateUIObject<UIPanel>(NAME("MenuItemContents"), Vec2i { 0, 0 }, UIObjectSize({ 80, UIObjectSize::PIXEL }, { 250, UIObjectSize::PIXEL }));
    m_container->SetIsVisible(false);
    m_container->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    m_container->SetBorderRadius(5);
    m_container->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    m_container->SetOriginAlignment(m_dropDirection == UIMenuBarDropDirection::DOWN ? UIObjectAlignment::TOP_LEFT : UIObjectAlignment::BOTTOM_LEFT);
    m_container->SetPadding({ 1, 1 });
    m_container->SetDepth(100);

    // @TODO: OnRemoved_Internal() , remove m_container from stage

    m_container->OnClick
        .Bind([this](const MouseEvent& data) -> UIEventHandlerResult
            {
                // Hide container on any item clicked
                SetSelectedMenuItemIndex(~0u);
                // Lose focus of the container (otherwise hovering over other menu items will cause the menu strips to reappear)
                Blur();

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    // m_container->OnLoseFocus.Bind([this](const MouseEvent &data) -> UIEventHandlerResult
    // {
    //     // First check that any child items aren't focused before hiding the container.
    //     if (!HasFocus(true)) {
    //         SetSelectedMenuItemIndex(~0u);
    //     }

    //     return UIEventHandlerResult::STOP_BUBBLING;
    // }).Detach();

    if (m_stage != nullptr)
    {
        m_stage->AddChildUIObject(m_container);
    }

    // AddChildUIObject(m_container);
}

void UIMenuBar::SetStage_Internal(UIStage* stage)
{
    UIPanel::SetStage_Internal(stage);

    if (m_container != nullptr)
    {
        m_container->RemoveFromParent();

        if (m_stage != nullptr)
        {
            m_stage->AddChildUIObject(m_container);
        }
    }
}

void UIMenuBar::OnRemoved_Internal()
{
    UIPanel::OnRemoved_Internal();

    if (m_container != nullptr)
    {
        m_container->RemoveFromParent();
    }
}

void UIMenuBar::SetDropDirection(UIMenuBarDropDirection dropDirection)
{
    m_dropDirection = dropDirection;

    if (m_container != nullptr)
    {
        if (m_dropDirection == UIMenuBarDropDirection::DOWN)
        {
            m_container->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
        }
        else
        {
            m_container->SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);
        }

        if (m_selectedMenuItemIndex != ~0u)
        {
            UIMenuItem* selectedMenuItem = m_menuItems[m_selectedMenuItemIndex];
            Assert(selectedMenuItem != nullptr);

            const Vec2i dropDownMenuPosition = GetDropDownMenuPosition(selectedMenuItem);

            m_container->SetPosition(dropDownMenuPosition);
        }
    }
}

void UIMenuBar::SetSelectedMenuItemIndex(uint32 index)
{
    Threads::AssertOnThread(g_gameThread);

    if (index == m_selectedMenuItemIndex)
    {
        return;
    }

    m_selectedMenuItemIndex = index;

    m_container->SetIsVisible(false);
    m_container->RemoveAllChildUIObjects();

    for (SizeType i = 0; i < m_menuItems.Size(); i++)
    {
        if (i == m_selectedMenuItemIndex)
        {
            continue;
        }

        UIMenuItem* menuItem = m_menuItems[i];

        if (!menuItem)
        {
            continue;
        }

        menuItem->SetFocusState(menuItem->GetFocusState() & ~UIObjectFocusState::TOGGLED);
    }

    if (m_selectedMenuItemIndex >= m_menuItems.Size())
    {
        m_selectedMenuItemIndex = ~0u;

        return;
    }

    UIMenuItem* menuItem = m_menuItems[m_selectedMenuItemIndex];

    if (!menuItem)
    {
        return;
    }

    menuItem->SetFocusState(menuItem->GetFocusState() | UIObjectFocusState::TOGGLED);

    m_container->AddChildUIObject(menuItem->GetDropDownMenuElement());
    m_container->SetSize(UIObjectSize({ menuItem->GetDropDownMenuElement()->GetActualSize().x + m_container->GetPadding().x * 2, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));

    m_container->SetPosition(GetDropDownMenuPosition(menuItem));
    m_container->SetIsVisible(true);
    m_container->Focus();
}

void UIMenuBar::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    if (!uiObject)
    {
        return;
    }

    if (!uiObject->IsA<UIMenuItem>() && !uiObject->IsA<UISpacer>())
    {
        HYP_LOG(UI, Warning, "Invalid object type to add to menu bar: {}", uiObject->InstanceClass()->GetName());

        return;
    }

    auto it = m_menuItems.FindAs(uiObject.Get());

    if (it != m_menuItems.End())
    {
        HYP_LOG(UI, Warning, "UIMenuBar::AddChildUIObject() called with a UIMenuItem that is already in the menu bar");

        return;
    }

    UIPanel::AddChildUIObject(uiObject);

    if (UIMenuItem* menuItem = ObjCast<UIMenuItem>(uiObject))
    {
        menuItem->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));

        const Name name = menuItem->GetName();

        // Mouse hover: set selected menu item index if this menu bar has focus
        menuItem->OnMouseHover.RemoveAllDetached();
        menuItem->OnMouseHover
            .Bind([this, name](const MouseEvent& data) -> UIEventHandlerResult
                {
                    if (m_container->HasFocus(true))
                    {
                        const uint32 menuItemIndex = GetMenuItemIndex(name);

                        SetSelectedMenuItemIndex(menuItemIndex);
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;
                })
            .Detach();

        // Mouse click: toggle selected menu item index
        // menuItem->OnClick.RemoveAllDetached();

        menuItem->OnClick
            .Bind([weakThis = WeakHandleFromThis(), name](const MouseEvent& data) -> UIEventHandlerResult
                {
                    Handle<UIMenuBar> menuBar = weakThis.Lock();

                    if (!menuBar)
                    {
                        return UIEventHandlerResult::OK;
                    }

                    if (data.mouseButtons == MouseButtonState::LEFT)
                    {
                        const uint32 menuItemIndex = menuBar->GetMenuItemIndex(name);

                        if (menuBar->GetSelectedMenuItemIndex() == menuItemIndex)
                        {
                            menuBar->SetSelectedMenuItemIndex(~0u);

                            menuBar->m_container->Blur();
                        }
                        else
                        {
                            menuBar->SetSelectedMenuItemIndex(menuItemIndex);
                        }
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;
                })
            .Detach();

        m_menuItems.PushBack(menuItem);
    }

    UpdateMenuItemSizes();
}

bool UIMenuBar::RemoveChildUIObject(UIObject* uiObject)
{
    if (!uiObject)
    {
        return false;
    }

    auto menuItemsIt = m_menuItems.FindAs(uiObject);

    if (menuItemsIt == m_menuItems.End())
    {
        return UIPanel::RemoveChildUIObject(uiObject);
    }

    const bool removed = UIPanel::RemoveChildUIObject(uiObject);

    if (!removed)
    {
        return false;
    }

    const SizeType index = menuItemsIt - m_menuItems.Begin();

    m_menuItems.Erase(menuItemsIt);

    UpdateMenuItemSizes();

    if (m_selectedMenuItemIndex == index)
    {
        SetSelectedMenuItemIndex(m_menuItems.Any() ? m_menuItems.Size() - 1 : ~0u);
    }

    return true;
}

void UIMenuBar::UpdateSize_Internal(bool updateChildren)
{
    UIPanel::UpdateSize_Internal(updateChildren);

    UpdateMenuItemSizes();
}

Handle<UIMenuItem> UIMenuBar::AddMenuItem(Name name, const String& text)
{
    Handle<UIMenuItem> menuItem = CreateUIObject<UIMenuItem>(name, Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
    menuItem->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    menuItem->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    menuItem->SetText(text);

    AddChildUIObject(menuItem);

    return menuItem;
}

UIMenuItem* UIMenuBar::GetMenuItem(Name name) const
{
    Threads::AssertOnThread(g_gameThread);

    for (UIMenuItem* menuItem : m_menuItems)
    {
        if (menuItem->GetName() == name)
        {
            return menuItem;
        }
    }

    return nullptr;
}

uint32 UIMenuBar::GetMenuItemIndex(Name name) const
{
    Threads::AssertOnThread(g_gameThread);

    for (SizeType i = 0; i < m_menuItems.Size(); i++)
    {
        if (m_menuItems[i]->GetName() == name)
        {
            return i;
        }
    }

    return ~0u;
}

bool UIMenuBar::RemoveMenuItem(Name name)
{
    Threads::AssertOnThread(g_gameThread);

    const auto it = m_menuItems.FindIf([name](UIMenuItem* menuItem)
        {
            return menuItem->GetName() == name;
        });

    if (it == m_menuItems.End())
    {
        return false;
    }

    return RemoveChildUIObject(*it);
}

void UIMenuBar::UpdateMenuItemSizes()
{
    // if (m_menuItems.Empty())
    // {
    //     return;
    // }

    // Vec2i offset = { 0, 0 };

    // for (SizeType i = 0; i < m_menuItems.Size(); i++)
    // {
    //     m_menuItems[i]->SetPosition(offset);
    //     m_menuItems[i]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));

    //     offset.x += m_menuItems[i]->GetActualSize().x;
    // }

    Array<UIObject*> childUiObjects = GetChildUIObjects(false);

    if (childUiObjects.Empty())
    {
        return;
    }

    for (SizeType i = 0; i < childUiObjects.Size(); i++)
    {
        UIObject* childUiObject = childUiObjects[i];
        Assert(childUiObject != nullptr);

        if (!childUiObject->IsA<UISpacer>())
        {
            childUiObject->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
        }
    }

    int totalNonSpacerWidth = 0;
    int numSpacers = 0;

    for (SizeType i = 0; i < childUiObjects.Size(); i++)
    {
        if (childUiObjects[i]->IsA<UISpacer>())
        {
            numSpacers++;

            continue;
        }

        totalNonSpacerWidth += childUiObjects[i]->GetActualSize().x;
    }

    Vec2i offset = { 0, 0 };

    const int availableWidth = GetActualSize().x - (GetPadding().x * 2);
    const int remainingWidth = MathUtil::Max(0, MathUtil::Floor(availableWidth - totalNonSpacerWidth));
    const int spacerWidth = numSpacers > 0 ? MathUtil::Ceil(remainingWidth / numSpacers) : 0;

    for (SizeType i = 0; i < childUiObjects.Size(); i++)
    {
        UIObject* childUiObject = childUiObjects[i];
        Assert(childUiObject != nullptr);

        childUiObject->SetPosition(offset);

        const Vec2i childPadding = childUiObjects[i]->GetPadding();

        if (UISpacer* spacer = ObjCast<UISpacer>(childUiObject))
        {
            spacer->SetSize(UIObjectSize({ spacerWidth, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));

            offset.x += spacerWidth + (childPadding.x * 2);

            continue;
        }

        offset.x += childUiObject->GetActualSize().x;
    }
}

Vec2i UIMenuBar::GetDropDownMenuPosition(UIMenuItem* menuItem) const
{
    Assert(menuItem != nullptr);
    Vec2f absolutePosition = menuItem->GetAbsolutePosition();

    if (m_dropDirection == UIMenuBarDropDirection::DOWN)
    {
        return Vec2i { int(absolutePosition.x), int(absolutePosition.y + menuItem->GetActualSize().y) };
    }
    else
    {
        return Vec2i { int(absolutePosition.x), int(absolutePosition.y) };
    }
}

#pragma region UIMenuBar

} // namespace hyperion
