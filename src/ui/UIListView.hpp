/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_LIST_VIEW_HPP
#define HYPERION_UI_LIST_VIEW_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

#pragma region UIListViewItem

class HYP_API UIListViewItem : public UIPanel
{
public:
    UIListViewItem(UIStage *stage, NodeProxy node_proxy);
    UIListViewItem(const UIListViewItem &other)                 = delete;
    UIListViewItem &operator=(const UIListViewItem &other)      = delete;
    UIListViewItem(UIListViewItem &&other) noexcept             = delete;
    UIListViewItem &operator=(UIListViewItem &&other) noexcept  = delete;
    virtual ~UIListViewItem() override                          = default;

    virtual void Init() override;
    
private:
    RC<UIObject>    m_inner_element;
};

#pragma endregion UIListViewItem

#pragma region UIListView

class HYP_API UIListView : public UIPanel
{
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
    HYP_NODISCARD HYP_FORCE_INLINE
    uint NumListViewItems() const
        { return m_list_view_items.Size(); }

    virtual void Init() override;

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    virtual void UpdateSize(bool update_children = true) override;

private:
    void UpdateLayout();

    Array<UIObject *>   m_list_view_items;
};

#pragma endregion UIListView

} // namespace hyperion

#endif