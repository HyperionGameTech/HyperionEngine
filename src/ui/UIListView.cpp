/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIListView.hpp>
#include <ui/UIDataSource.hpp>
#include <ui/UIStage.hpp>

#include <input/InputManager.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIListViewItem

UIListViewItem::UIListViewItem()
    : UIObject(UIObjectType::LIST_VIEW_ITEM),
      m_is_selected_item(false),
      m_is_expanded(true)
{
}

void UIListViewItem::Init()
{
    UIObject::Init();

    m_inner_element = CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    // Expand / collapse subitems when selected
    m_inner_element->OnClick.Bind([this](...)
    {
        if (HasSubItems()) {
            SetIsExpanded(!IsExpanded());
        }
        
        // allow bubbling up to the UIListViewItem parent
        return UIEventHandlerResult::OK;
    }).Detach();

    UIObject::AddChildUIObject(m_inner_element);
}

void UIListViewItem::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return;
    }
    
    if (ui_object->GetType() == UIObjectType::LIST_VIEW_ITEM) {
        if (!m_expanded_element) {
            m_expanded_element = CreateUIObject<UIListView>(Vec2i { 10, m_inner_element->GetActualSize().y }, UIObjectSize({ 100, UIObjectSize::FILL }, { 0, UIObjectSize::AUTO }));
            m_expanded_element->SetIsVisible(m_is_expanded);

            UIObject::AddChildUIObject(m_expanded_element);
        }

        m_expanded_element->AddChildUIObject(ui_object);

        return;
    }

    m_inner_element->AddChildUIObject(ui_object);
}

bool UIListViewItem::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }

    if (ui_object->GetType() == UIObjectType::LIST_VIEW_ITEM) {
        if (!m_expanded_element) {
            return false;
        }

        if (m_expanded_element->RemoveChildUIObject(ui_object)) {
            if (!HasSubItems()) {
                m_expanded_element->RemoveFromParent();
                m_expanded_element.Reset();
            }

            return true;
        }

        return false;
    }

    return m_inner_element->RemoveChildUIObject(ui_object);
}

bool UIListViewItem::HasSubItems() const
{
    if (!m_expanded_element) {
        return false;
    }

    return m_expanded_element->HasChildUIObjects();
}

void UIListViewItem::SetIsExpanded(bool is_expanded)
{
    if (is_expanded == m_is_expanded) {
        return;
    }

    if (is_expanded && !HasSubItems()) {
        // Can't expand if we don't have subitems
        return;
    }

    m_is_expanded = is_expanded;

    m_expanded_element->SetIsVisible(m_is_expanded);
}

void UIListViewItem::SetIsSelectedItem(bool is_selected_item)
{
    if (is_selected_item == m_is_selected_item) {
        return;
    }

    m_is_selected_item = is_selected_item;
}

void UIListViewItem::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    UIObject::SetFocusState_Internal(focus_state);

    if (focus_state & UIObjectFocusState::HOVER) {
        m_inner_element->SetBackgroundColor(Vec4f { 0.5f, 0.5f, 0.5f, 1.0f });
    } else {
        m_inner_element->SetBackgroundColor(Color(0x101012FFu));
    }
}

#pragma endregion UIListViewItem

#pragma region UIListView

UIListView::UIListView()
    : UIPanel(UIObjectType::LIST_VIEW)
{
    OnClick.Bind([this](...)
    {
        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();
}

void UIListView::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    AssertThrow(GetStage() != nullptr);
}

void UIListView::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return;
    }

    if (ui_object->GetType() == UIObjectType::LIST_VIEW_ITEM) {
        m_list_view_items.PushBack(ui_object.Cast<UIListViewItem>());

        UIObject::AddChildUIObject(ui_object);
    } else {
        RC<UIListViewItem> list_view_item = CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        list_view_item->AddChildUIObject(ui_object);

        m_list_view_items.PushBack(list_view_item);

        UIObject::AddChildUIObject(list_view_item);
    }

    UpdateSize(false);
}

bool UIListView::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }
    
    if (ui_object->GetClosestParentUIObject(UIObjectType::LIST_VIEW) == this) {
        bool removed = false;

        {
            UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

            for (auto it = m_list_view_items.Begin(); it != m_list_view_items.End();) {
                if (ui_object->IsOrHasParent(*it)) {
                    AssertThrow(UIObject::RemoveChildUIObject(ui_object));

                    it = m_list_view_items.Erase(it);

                    removed = true;
                } else {
                    ++it;
                }
            }
        }

        if (removed) {
            // Updates layout as well
            UpdateSize(false);

            return true;
        }

        HYP_LOG(UI, LogLevel::WARNING, "Failed to directly remove UIObject {} from ListView {}", ui_object->GetName(), GetName());
    }

    return UIObject::RemoveChildUIObject(ui_object);
}

void UIListView::UpdateSize_Internal(bool update_children)
{
    HYP_SCOPE;

    UIPanel::UpdateSize_Internal(update_children);

    UpdateLayout();
}

void UIListView::UpdateLayout()
{
    HYP_SCOPE;

    if (m_list_view_items.Empty()) {
        return;
    }

    int y_offset = 0;

    const Vec2i actual_size = GetActualSize();

    for (SizeType i = 0; i < m_list_view_items.Size(); i++) {
        UIObject *list_view_item = m_list_view_items[i];

        if (!list_view_item) {
            continue;
        }

        list_view_item->SetPosition({ 0, y_offset });

        y_offset += list_view_item->GetActualSize().y;
    }
}

void UIListView::SetDataSource_Internal(UIDataSourceBase *data_source)
{
    RemoveAllChildUIObjects();

    if (!data_source) {
        return;
    }

    // Add initial elements
    for (const auto &pair : data_source->GetValues()) {
        AddDataSourceElement(data_source, pair.first, pair.second);
    }

    m_data_source_on_element_add_handler = data_source->OnElementAdd.Bind([this, data_source](UIDataSourceBase *data_source_ptr, UIDataSourceElement *element, UIDataSourceElement *parent)
    {
        HYP_NAMED_SCOPE("Add element from data source to list view");

        AddDataSourceElement(data_source_ptr, element, parent);
    });

    m_data_source_on_element_remove_handler = data_source->OnElementRemove.Bind([this](UIDataSourceBase *data_source_ptr, UIDataSourceElement *element, UIDataSourceElement *parent)
    {
        HYP_NAMED_SCOPE("Remove element from data source from list view");

        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        HYP_DEFER({
            SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE);
        });

        UIListViewItem *list_view_item = FindListViewItem(element->GetUUID());

        if (list_view_item) {
            // If the item is selected, deselect it
            RC<UIListViewItem> selected_item_locked = m_selected_item.Lock();

            if (selected_item_locked == list_view_item) {
                selected_item_locked->SetIsSelectedItem(false);

                m_selected_item.Reset();

                OnSelectedItemChange(nullptr);
            }
            
            if (list_view_item->RemoveFromParent()) {
                return;
            }
        }

        HYP_LOG(UI, LogLevel::WARNING, "Failed to remove list view item with data source element UUID {}", element->GetUUID());
    });

    m_data_source_on_element_update_handler = data_source->OnElementUpdate.Bind([this, data_source](UIDataSourceBase *data_source_ptr, UIDataSourceElement *element, UIDataSourceElement *parent)
    {
        HYP_NAMED_SCOPE("Update element from data source in list view");

        HYP_LOG(UI, LogLevel::INFO, "Updating element {}", element->GetUUID().ToString());

        if (const UIListViewItem *list_view_item = FindListViewItem(element->GetUUID())) {
            if (RC<UIObject> ui_object = list_view_item->GetInnerElement()->GetChildUIObject(0)) {
                data_source->GetElementFactory()->UpdateUIObject(
                    ui_object.Get(),
                    element->GetValue()
                );

                // temp; sanity check
                AssertThrow(FindListViewItem(element->GetUUID()) != nullptr);
            } else {
                HYP_LOG(UI, LogLevel::ERR, "Failed to update element {}; No UIObject child at index 0", element->GetUUID().ToString());
            }
        } else {
            HYP_LOG(UI, LogLevel::WARNING, "Failed to update list view item with data source element UUID {}", element->GetUUID());
        }
    });
}

UIListViewItem *UIListView::FindListViewItem(const UUID &data_source_element_uuid) const
{
    return FindListViewItem(this, data_source_element_uuid);
}

UIListViewItem *UIListView::FindListViewItem(const UIObject *parent_object, const UUID &data_source_element_uuid)
{
    if (!parent_object) {
        return nullptr;
    }

    UIListViewItem *result_ptr = nullptr;

    parent_object->ForEachChildUIObject_Proc([&data_source_element_uuid, &result_ptr](UIObject *object)
    {
        if (object->GetType() == UIObjectType::LIST_VIEW_ITEM && object->GetDataSourceElementUUID() == data_source_element_uuid) {
            result_ptr = dynamic_cast<UIListViewItem *>(object);
            AssertThrow(result_ptr != nullptr);

            return UIObjectIterationResult::STOP;
        }

        if (UIListViewItem *result = FindListViewItem(object, data_source_element_uuid)) {
            result_ptr = result;

            return UIObjectIterationResult::STOP;
        }

        return UIObjectIterationResult::CONTINUE;
    }, false);

    return result_ptr;
}

void UIListView::AddDataSourceElement(UIDataSourceBase *data_source, UIDataSourceElement *element, UIDataSourceElement *parent)
{
    UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

    HYP_DEFER({
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE);
    });

    RC<UIListViewItem> list_view_item = CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    list_view_item->GetNode()->AddTag(NAME("DataSourceElementUUID"), NodeTag(element->GetUUID()));
    list_view_item->SetDataSourceElementUUID(element->GetUUID());
    
    list_view_item->OnClick.Bind([this, list_view_item_weak = list_view_item.ToWeak()](const MouseEvent &event) -> UIEventHandlerResult
    {
        RC<UIListViewItem> list_view_item = list_view_item_weak.Lock();

        if (!list_view_item) {
            return UIEventHandlerResult::ERR;
        }

        if (!list_view_item->HasFocus(true)) {
            // only call Focus if the item doesn't have focus (including children)
            list_view_item->Focus();
        }

        if (RC<UIListViewItem> selected_item = m_selected_item.Lock()) {
            selected_item->SetIsSelectedItem(false);
        }
        
        m_selected_item = list_view_item;

        list_view_item->SetIsSelectedItem(true);

        OnSelectedItemChange(list_view_item.Get());

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    // create UIObject for the element and add it to the list view
    list_view_item->AddChildUIObject(data_source->GetElementFactory()->CreateUIObject(GetStage(), element->GetValue()));

    if (parent) {
        // Child element - Find the parent
        UIListViewItem *parent_list_view_item = FindListViewItem(parent->GetUUID());

        if (parent_list_view_item) {
            parent_list_view_item->AddChildUIObject(list_view_item);

            return;
        } else {
            HYP_LOG(UI, LogLevel::WARNING, "Parent list view item not found, no list view item with data source element UUID {}", parent->GetUUID().ToString());
        }
    }

    // add the list view item to the list view
    AddChildUIObject(list_view_item);
}

#pragma region UIListView

} // namespace hyperion
