/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>
#include <ui/UIDataSource.hpp>

#include <input/InputManager.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIListViewItem

UIListViewItem::UIListViewItem(UIStage *parent, NodeProxy node_proxy)
    : UIObject(parent, std::move(node_proxy), UIObjectType::LIST_VIEW_ITEM),
      m_is_selected_item(false)
{
}

void UIListViewItem::Init()
{
    UIObject::Init();
}

void UIListViewItem::SetIsSelectedItem(bool is_selected_item)
{
    if (is_selected_item == m_is_selected_item) {
        return;
    }

    m_is_selected_item = is_selected_item;

    if (is_selected_item) {
        SetBackgroundColor(Vec4f { 0.25f, 0.25f, 0.25f, 1.0f });
    } else {
        SetBackgroundColor(Vec4f::Zero());
    }
}

void UIListViewItem::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    UIObject::SetFocusState_Internal(focus_state);

    if (!m_is_selected_item) {
        if (focus_state & UIObjectFocusState::HOVER) {
            SetBackgroundColor(Vec4f { 0.5f, 0.5f, 0.5f, 1.0f });
        } else {
            SetBackgroundColor(Vec4f::Zero());
        }
    }
}

#pragma endregion UIListViewItem

#pragma region UIListView

UIListView::UIListView(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::LIST_VIEW)
{
}

void UIListView::SetDataSource(UniquePtr<UIDataSourceBase> &&data_source)
{
    HYP_SCOPE;

    if (m_data_source) {
        m_data_source_on_change_handler.Reset();
        m_data_source_on_element_add_handler.Reset();
        m_data_source_on_element_remove_handler.Reset();
        m_data_source_on_element_update_handler.Reset();
    }

    m_data_source = std::move(data_source);

    if (!m_data_source) {
        return;
    }

    // @TODO initial setup of list view items!
    m_data_source_on_element_add_handler = m_data_source->OnElementAdd.Bind([this](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Add element from data source to list view");

        RC<UIListViewItem> list_view_item = GetStage()->CreateUIObject<UIListViewItem>(Name::Unique("ListViewItem"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        list_view_item->GetNode()->AddTag(NAME("DataSourceElementUUID"), NodeTag(element->GetUUID()));
        list_view_item->SetDataSourceElementUUID(element->GetUUID());
        
        list_view_item->OnClick.Bind([this, list_view_item_weak = list_view_item.ToWeak()](const MouseEvent &event) -> UIEventHandlerResult
        {
            RC<UIListViewItem> list_view_item = list_view_item_weak.Lock();

            if (!list_view_item) {
                return UIEventHandlerResult::ERR;
            }

            list_view_item->Focus();
            m_selected_item = list_view_item;

            OnSelectedItemChange.Broadcast(list_view_item.Get());

            return UIEventHandlerResult::OK;
        }).Detach();

        // create UIObject for the element and add it to the list view
        list_view_item->AddChildUIObject(m_data_source->GetElementFactory()->CreateUIObject(GetStage(), element->GetValue()));

        // add the list view item to the list view
        AddChildUIObject(list_view_item);
    });

    m_data_source_on_element_remove_handler = m_data_source->OnElementRemove.Bind([this](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Remove element from data source from list view");

        const auto it = m_list_view_items.FindIf([uuid = element->GetUUID()](UIObject *ui_object)
        {
            if (!ui_object) {
                return false;
            }

            // @TODO Only store UIListViewItem in m_list_view_items so we don't have to cast
            UIListViewItem *list_view_item = dynamic_cast<UIListViewItem *>(ui_object);

            if (!list_view_item) {
                return false;
            }

            return list_view_item->GetDataSourceElementUUID() == uuid;
        });

        if (it != m_list_view_items.End()) {
            // If the item is selected, deselect it
            if (m_selected_item.Lock().Get() == *it) {
                m_selected_item.Reset();

                OnSelectedItemChange.Broadcast(nullptr);
            }
            
            RemoveChildUIObject(*it);
        }
    });

    m_data_source_on_element_update_handler = m_data_source->OnElementUpdate.Bind([this](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Update element from data source in list view");

        HYP_LOG(UI, LogLevel::INFO, "Updating element {}", element->GetUUID().ToString());

        // update the list view item with the new data

        const auto it = m_list_view_items.FindIf([uuid = element->GetUUID()](UIObject *ui_object)
        {
            if (!ui_object) {
                return false;
            }

            // @TODO Only store UIListViewItem in m_list_view_items so we don't have to cast
            UIListViewItem *list_view_item = dynamic_cast<UIListViewItem *>(ui_object);

            if (!list_view_item) {
                return false;
            }

            return list_view_item->GetDataSourceElementUUID() == uuid;
        });

        // recreate the list view item content
        // @TODO: Make a function to update the content of a list view item
        if (it != m_list_view_items.End()) {
            // (*it)->RemoveAllChildUIObjects();
            // (*it)->AddChildUIObject(m_data_source->GetElementFactory()->CreateUIObject(
            //     GetStage(),
            //     element->GetValue()
            // ));

            if (const RC<UIObject> &ui_object = (*it)->GetChildUIObject(0)) {
                m_data_source->GetElementFactory()->UpdateUIObject(
                    ui_object.Get(),
                    element->GetValue()
                );
            } else {
                HYP_LOG(UI, LogLevel::ERR, "Failed to update element {}; No UIObject child at index 0", element->GetUUID().ToString());
            }
        }
    });
}

void UIListView::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    AssertThrow(GetStage() != nullptr);
}

void UIListView::AddChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    // RC<UIListViewItem> list_view_item = GetStage()->CreateUIObject<UIListViewItem>(Name::Unique("ListViewItem"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    // list_view_item->AddChildUIObject(ui_object);

    // m_list_view_items.PushBack(list_view_item);

    // UIObject::AddChildUIObject(std::move(list_view_item));

    if (!ui_object) {
        return;
    }

    m_list_view_items.PushBack(ui_object);

    UIObject::AddChildUIObject(ui_object);

    UpdateSize(false);
}

bool UIListView::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }
    
    for (SizeType i = 0; i < m_list_view_items.Size(); i++) {
        if (m_list_view_items[i] == ui_object) {
            AssertThrow(UIObject::RemoveChildUIObject(ui_object));

            m_list_view_items.EraseAt(i);

            // Updates layout as well
            UpdateSize(false);

            return true;
        }
    }

    return UIObject::RemoveChildUIObject(ui_object);
}

void UIListView::UpdateSize(bool update_children)
{
    HYP_SCOPE;

    UIPanel::UpdateSize(update_children);

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
    if (!data_source) {
        return;
    }

    m_data_source_on_element_add_handler = data_source->OnElementAdd.Bind([this, data_source](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Add element from data source to list view");

        RC<UIListViewItem> list_view_item = GetStage()->CreateUIObject<UIListViewItem>(NAME_FMT("ListViewItem_{}", m_list_view_items.Size()), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
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

            OnSelectedItemChange.Broadcast(list_view_item.Get());

            return UIEventHandlerResult::OK;
        }).Detach();

        // create UIObject for the element and add it to the list view
        list_view_item->AddChildUIObject(data_source->GetElementFactory()->CreateUIObject(GetStage(), element->GetValue()));

        // // add the list view item to the list view
        AddChildUIObject(list_view_item);

        // AddChildUIObject(data_source->GetElementFactory()->CreateUIObject(GetStage(), element->GetValue()));
    });

    m_data_source_on_element_remove_handler = data_source->OnElementRemove.Bind([this](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Remove element from data source from list view");

        const auto it = m_list_view_items.FindIf([uuid = element->GetUUID()](UIObject *ui_object)
        {
            if (!ui_object) {
                return false;
            }

            return ui_object->GetDataSourceElementUUID() == uuid;
        });

        if (it != m_list_view_items.End()) {
            // If the item is selected, deselect it
            if (m_selected_item.Lock().Get() == *it) {
                m_selected_item.Reset();

                OnSelectedItemChange.Broadcast(nullptr);
            }
            
            RemoveChildUIObject(*it);
        }
    });

    m_data_source_on_element_update_handler = data_source->OnElementUpdate.Bind([this, data_source](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Update element from data source in list view");

        HYP_LOG(UI, LogLevel::INFO, "Updating element {}", element->GetUUID().ToString());

        // update the list view item with the new data

        const auto it = m_list_view_items.FindIf([uuid = element->GetUUID()](UIObject *ui_object)
        {
            if (!ui_object) {
                return false;
            }

            // @TODO Only store UIListViewItem in m_list_view_items so we don't have to cast
            UIListViewItem *list_view_item = dynamic_cast<UIListViewItem *>(ui_object);

            if (!list_view_item) {
                return false;
            }

            return list_view_item->GetDataSourceElementUUID() == uuid;
        });

        // recreate the list view item content
        // @TODO: Make a function to update the content of a list view item
        if (it != m_list_view_items.End()) {
            // (*it)->RemoveAllChildUIObjects();
            // (*it)->AddChildUIObject(m_data_source->GetElementFactory()->CreateUIObject(
            //     GetStage(),
            //     element->GetValue()
            // ));

            // if (const RC<UIObject> &ui_object = (*it)->GetChildUIObject(0)) {
                data_source->GetElementFactory()->UpdateUIObject(
                    *it,//ui_object.Get(),
                    element->GetValue()
                );
            // } else {
            //     HYP_LOG(UI, LogLevel::ERROR, "Failed to update element {}; No UIObject child at index 0", element->GetUUID().ToString());
            // }
        }
    });
}

#pragma region UIListView

} // namespace hyperion
