/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_LIST_VIEW_HPP
#define HYPERION_UI_LIST_VIEW_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

namespace hyperion {

#pragma region UIListViewItem

HYP_CLASS()
class HYP_API UIListViewItem : public UIObject
{
    HYP_OBJECT_BODY(UIListViewItem);

public:
    friend class UIListView;

    UIListViewItem(UIStage *stage, NodeProxy node_proxy);
    UIListViewItem(const UIListViewItem &other)                 = delete;
    UIListViewItem &operator=(const UIListViewItem &other)      = delete;
    UIListViewItem(UIListViewItem &&other) noexcept             = delete;
    UIListViewItem &operator=(UIListViewItem &&other) noexcept  = delete;
    virtual ~UIListViewItem() override                          = default;

    virtual void Init() override;

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    bool HasSubItems() const;

    HYP_FORCE_INLINE bool IsExpanded() const
        { return m_is_expanded; }

    void SetIsExpanded(bool is_expanded);

protected:
    virtual void SetIsSelectedItem(bool is_selected_item);

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state) override;

private:
    RC<UIObject>    m_inner_element;
    RC<UIObject>    m_expanded_element;
    bool            m_is_selected_item;
    bool            m_is_expanded;
};

#pragma endregion UIListViewItem

#pragma region UIListView

HYP_CLASS()
class HYP_API UIListView : public UIPanel
{
    HYP_OBJECT_BODY(UIListView);

public:
    UIListView(UIStage *stage, NodeProxy node_proxy);
    UIListView(const UIListView &other)                 = delete;
    UIListView &operator=(const UIListView &other)      = delete;
    UIListView(UIListView &&other) noexcept             = delete;
    UIListView &operator=(UIListView &&other) noexcept  = delete;
    virtual ~UIListView() override                      = default;

    /*! \brief Get the number of items in the list view.
     * 
     * \return The number of items in the list view. */
    HYP_FORCE_INLINE uint NumListViewItems() const
        { return m_list_view_items.Size(); }

    virtual void Init() override;

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    virtual void UpdateSize(bool update_children = true) override;

    Delegate<void, UIListViewItem *>    OnSelectedItemChange;

protected:
    virtual void SetDataSource_Internal(UIDataSourceBase *data_source) override;

    void UpdateLayout();

    Array<UIObject *>           m_list_view_items;
    Weak<UIListViewItem>        m_selected_item;
};

#pragma endregion UIListView

} // namespace hyperion

#endif