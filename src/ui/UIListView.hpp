/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_LIST_VIEW_HPP
#define HYPERION_UI_LIST_VIEW_HPP

#include <ui/UIPanel.hpp>
#include <ui/UIDataSource.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

namespace hyperion {

#pragma region UIListViewItem

class HYP_API UIListViewItem : public UIPanel
{
public:
    friend class UIListView;

    UIListViewItem(UIStage *stage, NodeProxy node_proxy);
    UIListViewItem(const UIListViewItem &other)                 = delete;
    UIListViewItem &operator=(const UIListViewItem &other)      = delete;
    UIListViewItem(UIListViewItem &&other) noexcept             = delete;
    UIListViewItem &operator=(UIListViewItem &&other) noexcept  = delete;
    virtual ~UIListViewItem() override                          = default;

    /*! \brief Gets the UUID of the associated data source element (if applicable).
     *  Otherwise, returns an empty UUID.
     * 
     * \return The UUID of the associated data source element. */
    HYP_FORCE_INLINE
    UUID GetDataSourceElementUUID() const
        { return m_data_source_element_uuid; }

    virtual void Init() override;
    
private:
    /*! \brief Sets the UUID of the associated data source element.
     *  \internal This is used by the UIListView to set the UUID of the associated data source element.
     * 
     * \param data_source_element_uuid The UUID of the associated data source element. */
    HYP_FORCE_INLINE
    void SetDataSourceElementUUID(UUID data_source_element_uuid)
        { m_data_source_element_uuid = data_source_element_uuid; }

    RC<UIObject>    m_inner_element;
    UUID            m_data_source_element_uuid;
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

    HYP_FORCE_INLINE
    UIDataSourceBase *GetDataSource() const
        { return m_data_source.Get(); }

    template <class T>
    HYP_FORCE_INLINE
    UIDataSource<T> *GetDataSource() const
    {
        if (!m_data_source.IsDynamic<UIDataSource<T>>()) {
            return nullptr;
        }

        return static_cast<UIDataSource<T> *>(m_data_source.Get());
    }

    void SetDataSource(UniquePtr<UIDataSourceBase> &&data_source);

    virtual void Init() override;

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    virtual void UpdateSize(bool update_children = true) override;

    Delegate<void, UIListViewItem *>    OnSelectedItemChange;

protected:
    void UpdateLayout();

    Array<UIObject *>           m_list_view_items;
    Weak<UIListViewItem>        m_selected_item;

    UniquePtr<UIDataSourceBase> m_data_source;
    DelegateHandler             m_data_source_on_change_handler;
    DelegateHandler             m_data_source_on_element_add_handler;
    DelegateHandler             m_data_source_on_element_remove_handler;
    DelegateHandler             m_data_source_on_element_update_handler;
};

#pragma endregion UIListView

} // namespace hyperion

#endif