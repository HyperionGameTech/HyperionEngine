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
    : UIObject(UIObjectType::LIST_VIEW_ITEM),
      m_expanded_element(nullptr),
      m_is_selected_item(false),
      m_is_expanded(false)
{
    SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));

    OnClick.Bind([this](...)
    {
        if (HasSubItems()) {
            SetIsExpanded(!IsExpanded());
        }
        
        // allow bubbling up to the UIListViewItem parent
        return UIEventHandlerResult::OK;
    }).Detach();
}

void UIListViewItem::Init()
{
    UIObject::Init();

    m_initial_background_color = GetBackgroundColor();
}

void UIListViewItem::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return;
    }
    
    if (ui_object->IsInstanceOf<UIListViewItem>()) {
        if (!m_expanded_element) {
            RC<UIListView> expanded_element = CreateUIObject<UIListView>(Vec2i { 10, GetActualSize().y }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
            expanded_element->SetIsVisible(m_is_expanded);

            UIObject::AddChildUIObject(expanded_element);

            m_expanded_element = expanded_element.Get();
        }

        m_expanded_element->AddChildUIObject(ui_object);

        return;
    }

    UIObject::AddChildUIObject(ui_object);
}

bool UIListViewItem::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }

    if (ui_object->IsInstanceOf<UIListViewItem>()) {
        if (!m_expanded_element) {
            return false;
        }

        if (m_expanded_element->RemoveChildUIObject(ui_object)) {
            if (!HasSubItems()) {
                AssertThrow(UIObject::RemoveChildUIObject(m_expanded_element));

                m_expanded_element = nullptr;
            }

            return true;
        }

        return false;
    }

    return UIObject::RemoveChildUIObject(ui_object);
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

    UpdateMaterial(false);
}

void UIListViewItem::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    UIObject::SetFocusState_Internal(focus_state);

    UpdateMaterial(false);
}

Material::ParameterTable UIListViewItem::GetMaterialParameters() const
{
    Color color = GetBackgroundColor();

    const EnumFlags<UIObjectFocusState> focus_state = GetFocusState();

    if (m_is_selected_item) {
        color = Color(Vec4f { 0.5f, 0.5f, 0.5f, 1.0f });
    } else if (focus_state & UIObjectFocusState::HOVER) {
        color = Color(Vec4f { 0.3f, 0.3f, 0.3f, 1.0f });
    }

    return Material::ParameterTable {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(color) }
    };
}

#pragma endregion UIListViewItem

#pragma region UIListView

UIListView::UIListView()
    : UIPanel(UIObjectType::LIST_VIEW),
      m_orientation(UIListViewOrientation::VERTICAL)
{
    OnClick.Bind([this](...)
    {
        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();
}

UIListView::~UIListView()
{
    m_list_view_items.Clear();
}

void UIListView::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    UIPanel::Init();
}

void UIListView::SetOrientation(UIListViewOrientation orientation)
{
    HYP_SCOPE;

    if (m_orientation == orientation) {
        return;
    }

    m_orientation = orientation;
    
    for (UIListViewItem *list_view_item : m_list_view_items) {
        UILockedUpdatesScope scope(*list_view_item, UIObjectUpdateType::UPDATE_SIZE);

        if (m_orientation == UIListViewOrientation::VERTICAL) {
            list_view_item->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        } else {
            list_view_item->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
        }
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);
}

void UIListView::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return;
    }

    UIObjectSize list_view_item_size;

    if (m_orientation == UIListViewOrientation::HORIZONTAL) {
        list_view_item_size = UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT });
    } else {
        list_view_item_size = UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO });
    }

    RC<UIListViewItem> list_view_item;

    if (ui_object->IsInstanceOf<UIListViewItem>()) {
        list_view_item = ui_object.CastUnsafe<UIListViewItem>();
        list_view_item->SetSize(list_view_item_size);
        m_list_view_items.PushBack(list_view_item);

        UIObject::AddChildUIObject(ui_object);
    } else {
        list_view_item = CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, list_view_item_size);
        list_view_item->AddChildUIObject(ui_object);

        m_list_view_items.PushBack(list_view_item);

        UIObject::AddChildUIObject(list_view_item);
    }

    UpdateLayout();
}

bool UIListView::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }

    if (ui_object->IsInstanceOf<UIListViewItem>()) {
        auto it = m_list_view_items.FindAs(ui_object);

        if (it != m_list_view_items.End()) {
            m_list_view_items.Erase(it);
        }
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

    const Vec2i offset_multiplier = m_orientation == UIListViewOrientation::VERTICAL ? Vec2i { 0, 1 } : Vec2i { 1, 0 };

    Vec2i offset;

    for (SizeType i = 0; i < m_list_view_items.Size(); i++) {
        UIObject *list_view_item = m_list_view_items[i];

        if (!list_view_item) {
            continue;
        }

        {
            UILockedUpdatesScope scope(*list_view_item, UIObjectUpdateType::UPDATE_SIZE);

            list_view_item->SetPosition(offset);
        }

        offset += list_view_item->GetActualSize() * offset_multiplier;
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
            if (RC<UIListViewItem> selected_item = m_selected_item.Lock(); list_view_item == selected_item.Get()) {
                selected_item->SetIsSelectedItem(false);
                m_selected_item.Reset();

                OnSelectedItemChange(nullptr);
            }
            
            if (list_view_item->RemoveFromParent()) {
                return;
            }
        }

        HYP_LOG(UI, Warning, "Failed to remove list view item with data source element UUID {}", element->GetUUID());
    });

    m_data_source_on_element_update_handler = data_source->OnElementUpdate.Bind([this, data_source](UIDataSourceBase *data_source_ptr, UIDataSourceElement *element, UIDataSourceElement *parent)
    {
        HYP_NAMED_SCOPE("Update element from data source in list view");

        HYP_LOG(UI, Info, "Updating element {}", element->GetUUID().ToString());

        if (const UIListViewItem *list_view_item = FindListViewItem(element->GetUUID())) {
            if (RC<UIObject> ui_object = list_view_item->GetChildUIObject(0)) {
                data_source->UpdateUIObject(ui_object.Get(), element->GetValue(), { });
            } else {
                HYP_LOG(UI, Error, "Failed to update element {}; No UIObject child at index 0", element->GetUUID().ToString());
            }
        } else {
            HYP_LOG(UI, Warning, "Failed to update list view item with data source element UUID {}", element->GetUUID());
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
        if (object->IsInstanceOf<UIListViewItem>() && object->GetDataSourceElementUUID() == data_source_element_uuid) {
            result_ptr = dynamic_cast<UIListViewItem *>(object);
            AssertThrow(result_ptr != nullptr);

            return IterationResult::STOP;
        }

        if (UIListViewItem *result = FindListViewItem(object, data_source_element_uuid)) {
            result_ptr = result;

            return IterationResult::STOP;
        }

        return IterationResult::CONTINUE;
    }, false);

    return result_ptr;
}

void UIListView::AddDataSourceElement(UIDataSourceBase *data_source, UIDataSourceElement *element, UIDataSourceElement *parent)
{
    UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

    RC<UIListViewItem> list_view_item = CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    list_view_item->SetNodeTag(NodeTag(NAME("DataSourceElementUUID"), element->GetUUID()));
    list_view_item->SetDataSourceElementUUID(element->GetUUID());
    
    list_view_item->OnClick.Bind([this, list_view_item_weak = list_view_item.ToWeak()](const MouseEvent &event) -> UIEventHandlerResult
    {
        RC<UIListViewItem> list_view_item = list_view_item_weak.Lock();

        if (!list_view_item) {
            return UIEventHandlerResult::ERR;
        }

        SetSelectedItem(list_view_item.Get());

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    // create UIObject for the element and add it to the list view
    list_view_item->AddChildUIObject(data_source->CreateUIObject(list_view_item, element->GetValue(), { }));

    if (parent) {
        // Child element - Find the parent
        UIListViewItem *parent_list_view_item = FindListViewItem(parent->GetUUID());

        if (parent_list_view_item) {
            parent_list_view_item->AddChildUIObject(list_view_item);

            SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);

            return;
        } else {
            HYP_LOG(UI, Warning, "Parent list view item not found, no list view item with data source element UUID {}", parent->GetUUID().ToString());
        }
    }

    // add the list view item to the list view
    AddChildUIObject(list_view_item);
}

void UIListView::SetSelectedItemIndex(int index)
{
    HYP_SCOPE;

    if (index < 0 || index >= m_list_view_items.Size()) {
        return;
    }

    UIListViewItem *list_view_item = m_list_view_items[index];

    if (m_selected_item.GetUnsafe() == list_view_item) {
        return;
    }

    if (RC<UIListViewItem> selected_item = m_selected_item.Lock()) {
        selected_item->SetIsSelectedItem(false);
    }

    list_view_item->SetIsSelectedItem(true);

    m_selected_item = list_view_item->WeakRefCountedPtrFromThis().CastUnsafe<UIListViewItem>();

    OnSelectedItemChange(list_view_item);
}

void UIListView::SetSelectedItem(UIListViewItem *list_view_item)
{
    HYP_SCOPE;

    if (m_selected_item.GetUnsafe() == list_view_item) {
        return;
    }

    // List view item must have this as a parent
    if (!list_view_item || !list_view_item->IsOrHasParent(this)) {
        SetSelectedItemIndex(-1);

        return;
    }

    if (RC<UIListViewItem> selected_item = m_selected_item.Lock()) {
        selected_item->SetIsSelectedItem(false);
    }

    if (!list_view_item->HasFocus()) {
        list_view_item->Focus();
    }

    list_view_item->SetIsSelectedItem(true);

    if (list_view_item->GetParentUIObject() != this) {
        // Iterate until we reach this
        UIObject *parent = list_view_item->GetParentUIObject();

        bool is_expanded = false;

        while (parent != nullptr && parent != this) {
            if (parent->IsInstanceOf<UIListViewItem>()) {
                UIListViewItem *parent_list_view_item = static_cast<UIListViewItem *>(parent);

                if (!parent_list_view_item->IsExpanded()) {
                    parent_list_view_item->SetIsExpanded(true);

                    is_expanded = true;
                }
            }

            parent = parent->GetParentUIObject();
        }

        // Force update of the list view and children after expanding items.
        if (is_expanded) {
            UpdateSize();
        }
    }

    ScrollToChild(list_view_item);

    m_selected_item = list_view_item->WeakRefCountedPtrFromThis().CastUnsafe<UIListViewItem>();

    OnSelectedItemChange(list_view_item);
}

#pragma region UIListView

} // namespace hyperion
