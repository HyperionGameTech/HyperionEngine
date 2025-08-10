/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIListView.hpp>
#include <ui/UIDataSource.hpp>
#include <ui/UIStage.hpp>

#include <input/InputManager.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIListViewItem

UIListViewItem::UIListViewItem()
    : m_expandedElement(nullptr),
      m_isSelectedItem(false),
      m_isExpanded(false)
{
    SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));

    OnClick
        .Bind([this](...)
            {
                if (HasSubItems())
                {
                    SetIsExpanded(!IsExpanded());
                }

                // allow bubbling up to the UIListViewItem parent
                return UIEventHandlerResult::OK;
            })
        .Detach();
}

void UIListViewItem::Init()
{
    UIObject::Init();

    m_initialBackgroundColor = GetBackgroundColor();
}

void UIListViewItem::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return;
    }

    if (uiObject->IsA<UIListViewItem>())
    {
        if (!m_expandedElement)
        {
            Handle<UIListView> expandedElement = CreateUIObject<UIListView>(Vec2i { 10, GetActualSize().y }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
            expandedElement->SetIsVisible(m_isExpanded);

            UIObject::AddChildUIObject(expandedElement);

            m_expandedElement = expandedElement.Get();
        }

        m_expandedElement->AddChildUIObject(uiObject);

        return;
    }

    UIObject::AddChildUIObject(uiObject);
}

bool UIListViewItem::RemoveChildUIObject(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return false;
    }

    if (uiObject->IsA<UIListViewItem>())
    {
        if (!m_expandedElement)
        {
            return false;
        }

        if (m_expandedElement->RemoveChildUIObject(uiObject))
        {
            if (!HasSubItems())
            {
                Assert(UIObject::RemoveChildUIObject(m_expandedElement));

                m_expandedElement = nullptr;
            }

            return true;
        }

        return false;
    }

    return UIObject::RemoveChildUIObject(uiObject);
}

bool UIListViewItem::HasSubItems() const
{
    if (!m_expandedElement)
    {
        return false;
    }

    return m_expandedElement->HasChildUIObjects();
}

void UIListViewItem::SetIsExpanded(bool isExpanded)
{
    if (isExpanded == m_isExpanded)
    {
        return;
    }

    if (isExpanded && !HasSubItems())
    {
        // Can't expand if we don't have subitems
        return;
    }

    m_isExpanded = isExpanded;

    m_expandedElement->SetIsVisible(m_isExpanded);
    // UpdateSize();
}

void UIListViewItem::SetIsSelectedItem(bool isSelectedItem)
{
    if (isSelectedItem == m_isSelectedItem)
    {
        return;
    }

    m_isSelectedItem = isSelectedItem;

    UpdateMaterial(false);
}

void UIListViewItem::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState)
{
    UIObject::SetFocusState_Internal(focusState);

    UpdateMaterial(false);
}

Material::ParameterTable UIListViewItem::GetMaterialParameters() const
{
    Color color = GetBackgroundColor();

    const EnumFlags<UIObjectFocusState> focusState = GetFocusState();

    if (m_isSelectedItem)
    {
        color = Color(Vec4f { 0.5f, 0.5f, 0.5f, 1.0f });
    }
    else if (focusState & UIObjectFocusState::HOVER)
    {
        color = Color(Vec4f { 0.3f, 0.3f, 0.3f, 1.0f });
    }

    return Material::ParameterTable {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(color) }
    };
}

#pragma endregion UIListViewItem

#pragma region UIListView

UIListView::UIListView()
    : m_orientation(UIListViewOrientation::VERTICAL)
{
    OnClick.Bind([this](...)
               {
                   return UIEventHandlerResult::STOP_BUBBLING;
               })
        .Detach();
}

UIListView::~UIListView()
{
    m_listViewItems.Clear();
}

void UIListView::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    UIPanel::Init();
}

void UIListView::SetOrientation(UIListViewOrientation orientation)
{
    HYP_SCOPE;

    if (m_orientation == orientation)
    {
        return;
    }

    m_orientation = orientation;

    for (UIListViewItem* listViewItem : m_listViewItems)
    {
        UILockedUpdatesScope scope(*listViewItem, UIObjectUpdateType::UPDATE_SIZE);

        if (m_orientation == UIListViewOrientation::VERTICAL)
        {
            listViewItem->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        }
        else
        {
            listViewItem->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
        }
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);
}

void UIListView::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return;
    }

    UIObjectSize listViewItemSize;

    if (m_orientation == UIListViewOrientation::HORIZONTAL)
    {
        listViewItemSize = UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT });
    }
    else
    {
        listViewItemSize = UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO });
    }

    Handle<UIListViewItem> listViewItem;

    if (uiObject->IsA<UIListViewItem>())
    {
        listViewItem = ObjCast<UIListViewItem>(uiObject);
        listViewItem->SetSize(listViewItemSize);
        m_listViewItems.PushBack(listViewItem);

        UIObject::AddChildUIObject(uiObject);
    }
    else
    {
        listViewItem = CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, listViewItemSize);
        listViewItem->AddChildUIObject(uiObject);

        m_listViewItems.PushBack(listViewItem);

        UIObject::AddChildUIObject(listViewItem);
    }

    UpdateLayout();
}

bool UIListView::RemoveChildUIObject(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return false;
    }

    if (uiObject->IsA<UIListViewItem>())
    {
        auto it = m_listViewItems.FindAs(uiObject);

        if (it != m_listViewItems.End())
        {
            m_listViewItems.Erase(it);
        }
    }

    return UIObject::RemoveChildUIObject(uiObject);
}

void UIListView::UpdateSize_Internal(bool updateChildren)
{
    HYP_SCOPE;

    UIPanel::UpdateSize_Internal(updateChildren);

    UpdateLayout();
}

void UIListView::UpdateLayout()
{
    HYP_SCOPE;

    if (m_listViewItems.Empty())
    {
        return;
    }

    const Vec2i offsetMultiplier = m_orientation == UIListViewOrientation::VERTICAL ? Vec2i { 0, 1 } : Vec2i { 1, 0 };

    Vec2i offset;

    for (SizeType i = 0; i < m_listViewItems.Size(); i++)
    {
        UIObject* listViewItem = m_listViewItems[i];

        if (!listViewItem)
        {
            continue;
        }

        {
            UILockedUpdatesScope scope(*listViewItem, UIObjectUpdateType::UPDATE_SIZE);

            listViewItem->SetPosition(offset);
        }

        offset += listViewItem->GetActualSize() * offsetMultiplier;
    }
}

void UIListView::SetDataSource_Internal(UIDataSourceBase* dataSource)
{
    RemoveAllChildUIObjects();

    if (!dataSource)
    {
        return;
    }

    // Add initial elements
    for (const auto& pair : dataSource->GetValues())
    {
        AddDataSourceElement(dataSource, pair.first, pair.second);
    }

    m_dataSourceOnElementAddHandler = dataSource->OnElementAdd.Bind([this, dataSource](UIDataSourceBase* dataSourcePtr, UIDataSourceElement* element, UIDataSourceElement* parent)
        {
            HYP_NAMED_SCOPE("Add element from data source to list view");

            AddDataSourceElement(dataSourcePtr, element, parent);
        });

    m_dataSourceOnElementRemoveHandler = dataSource->OnElementRemove.Bind([this](UIDataSourceBase* dataSourcePtr, UIDataSourceElement* element, UIDataSourceElement* parent)
        {
            HYP_NAMED_SCOPE("Remove element from data source from list view");

            UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

            HYP_DEFER({
                SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE);
            });

            UIListViewItem* listViewItem = FindListViewItem(element->GetUUID());

            if (listViewItem)
            {
                // If the item is selected, deselect it
                if (Handle<UIListViewItem> selectedItem = m_selectedItem.Lock(); listViewItem == selectedItem.Get())
                {
                    selectedItem->SetIsSelectedItem(false);
                    m_selectedItem.Reset();

                    OnSelectedItemChange(nullptr);
                }

                if (listViewItem->RemoveFromParent())
                {
                    return;
                }
            }

            HYP_LOG(UI, Warning, "Failed to remove list view item with data source element UUID {}", element->GetUUID());
        });

    m_dataSourceOnElementUpdateHandler = dataSource->OnElementUpdate.Bind([this, dataSource](UIDataSourceBase* dataSourcePtr, UIDataSourceElement* element, UIDataSourceElement* parent)
        {
            HYP_NAMED_SCOPE("Update element from data source in list view");

            HYP_LOG(UI, Info, "Updating element {}", element->GetUUID().ToString());

            if (const UIListViewItem* listViewItem = FindListViewItem(element->GetUUID()))
            {
                if (Handle<UIObject> uiObject = listViewItem->GetChildUIObject(0))
                {
                    dataSource->UpdateUIObject(uiObject.Get(), element->GetValue(), {});
                }
                else
                {
                    HYP_LOG(UI, Error, "Failed to update element {}; No UIObject child at index 0", element->GetUUID().ToString());
                }
            }
            else
            {
                HYP_LOG(UI, Warning, "Failed to update list view item with data source element UUID {}", element->GetUUID());
            }
        });
}

UIListViewItem* UIListView::FindListViewItem(const UUID& dataSourceElementUuid) const
{
    return FindListViewItem(this, dataSourceElementUuid);
}

UIListViewItem* UIListView::FindListViewItem(const UIObject* parentObject, const UUID& dataSourceElementUuid)
{
    if (!parentObject)
    {
        return nullptr;
    }

    UIListViewItem* resultPtr = nullptr;

    parentObject->ForEachChildUIObject_Proc([&dataSourceElementUuid, &resultPtr](UIObject* object)
        {
            if (object->IsA<UIListViewItem>() && object->GetDataSourceElementUUID() == dataSourceElementUuid)
            {
                resultPtr = ObjCast<UIListViewItem>(object);

                return IterationResult::STOP;
            }

            if (UIListViewItem* result = FindListViewItem(object, dataSourceElementUuid))
            {
                resultPtr = result;

                return IterationResult::STOP;
            }

            return IterationResult::CONTINUE;
        },
        false);

    return resultPtr;
}

void UIListView::AddDataSourceElement(UIDataSourceBase* dataSource, UIDataSourceElement* element, UIDataSourceElement* parent)
{
    UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

    Handle<UIListViewItem> listViewItem = CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    listViewItem->AddTag(NodeTag(NAME("DataSourceElementUUID"), element->GetUUID()));
    listViewItem->SetDataSourceElementUUID(element->GetUUID());

    listViewItem->OnClick
        .Bind([this, listViewItemWeak = listViewItem.ToWeak()](const MouseEvent& event) -> UIEventHandlerResult
            {
                Handle<UIListViewItem> listViewItem = listViewItemWeak.Lock();

                if (!listViewItem)
                {
                    return UIEventHandlerResult::ERR;
                }

                SetSelectedItem(listViewItem.Get());

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    // create UIObject for the element and add it to the list view
    listViewItem->AddChildUIObject(dataSource->CreateUIObject(listViewItem, element->GetValue(), {}));

    if (parent)
    {
        // Child element - Find the parent
        UIListViewItem* parentListViewItem = FindListViewItem(parent->GetUUID());

        if (parentListViewItem)
        {
            parentListViewItem->AddChildUIObject(listViewItem);

            SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);

            return;
        }
        else
        {
            HYP_LOG(UI, Warning, "Parent list view item not found, no list view item with data source element UUID {}", parent->GetUUID().ToString());
        }
    }

    // add the list view item to the list view
    AddChildUIObject(listViewItem);
}

void UIListView::SetSelectedItemIndex(int index)
{
    HYP_SCOPE;

    if (index < 0 || index >= m_listViewItems.Size())
    {
        return;
    }

    UIListViewItem* listViewItem = m_listViewItems[index];

    if (m_selectedItem.GetUnsafe() == listViewItem)
    {
        return;
    }

    if (Handle<UIListViewItem> selectedItem = m_selectedItem.Lock())
    {
        selectedItem->SetIsSelectedItem(false);
    }

    listViewItem->SetIsSelectedItem(true);

    m_selectedItem = listViewItem->WeakHandleFromThis();

    OnSelectedItemChange(listViewItem);
}

void UIListView::SetSelectedItem(UIListViewItem* listViewItem)
{
    HYP_SCOPE;

    if (m_selectedItem.GetUnsafe() == listViewItem)
    {
        return;
    }

    // List view item must have this as a parent
    if (!listViewItem || !listViewItem->IsOrHasParent(this))
    {
        SetSelectedItemIndex(-1);

        return;
    }

    if (Handle<UIListViewItem> selectedItem = m_selectedItem.Lock())
    {
        selectedItem->SetIsSelectedItem(false);
    }

    if (!listViewItem->HasFocus())
    {
        listViewItem->Focus();
    }

    listViewItem->SetIsSelectedItem(true);

    if (listViewItem->GetParentUIObject() != this)
    {
        // Iterate until we reach this
        UIObject* parent = listViewItem->GetParentUIObject();

        bool isExpanded = false;

        while (parent != nullptr && parent != this)
        {
            if (UIListViewItem* parentListViewItem = ObjCast<UIListViewItem>(parent))
            {
                if (!parentListViewItem->IsExpanded())
                {
                    parentListViewItem->SetIsExpanded(true);

                    isExpanded = true;
                }
            }

            parent = parent->GetParentUIObject();
        }

        // Force update of the list view and children after expanding items.
        if (isExpanded)
        {
            UpdateSize();
        }
    }

    ScrollToChild(listViewItem);

    m_selectedItem = listViewItem->WeakHandleFromThis();

    OnSelectedItemChange(listViewItem);
}

#pragma region UIListView

} // namespace hyperion
