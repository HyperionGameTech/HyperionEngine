/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_GRID_HPP
#define HYPERION_UI_GRID_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

#pragma region UIGridColumn

HYP_CLASS()
class HYP_API UIGridColumn : public UIPanel
{
    HYP_OBJECT_BODY(UIGridColumn);

public:
    UIGridColumn(UIStage *stage, NodeProxy node_proxy);
    UIGridColumn(const UIGridColumn &other)                 = delete;
    UIGridColumn &operator=(const UIGridColumn &other)      = delete;
    UIGridColumn(UIGridColumn &&other) noexcept             = delete;
    UIGridColumn &operator=(UIGridColumn &&other) noexcept  = delete;
    virtual ~UIGridColumn() override                        = default;

    virtual void Init() override;
};

#pragma endregion UIGridColumn

#pragma region UIGridRow

HYP_CLASS()
class HYP_API UIGridRow : public UIPanel
{
    HYP_OBJECT_BODY(UIGridRow);

public:
    UIGridRow(UIStage *stage, NodeProxy node_proxy);
    UIGridRow(const UIGridRow &other)                   = delete;
    UIGridRow &operator=(const UIGridRow &other)        = delete;
    UIGridRow(UIGridRow &&other) noexcept               = delete;
    UIGridRow &operator=(UIGridRow &&other) noexcept    = delete;
    virtual ~UIGridRow() override                       = default;

    /*! \brief Gets the columns in the row.
     * 
     * \return A array of reference counted pointers to the columns in the row. */
    HYP_FORCE_INLINE const Array<RC<UIGridColumn>> &GetColumns() const
        { return m_columns; }

    /*! \brief Gets the number of columns in the row.
     * 
     * \return The number of columns in the row. */
    HYP_FORCE_INLINE SizeType GetNumColumns() const
        { return m_columns.Size(); }

    /*! \brief Sets the number of columns in the row.
     * 
     * \param num_columns The number of columns to set. */
    void SetNumColumns(SizeType num_columns);
    
    /*! \brief Adds a new column to the row.
     * 
     * \return A reference counted pointer to the newly created column. */
    RC<UIGridColumn> AddColumn();

    /*! \brief Gets the column at the specified index.
     * 
     * \param index The index of the column to retrieve.
     * \return A reference counted pointer to the column at the specified index.
     *  If the index is out of bounds, a null pointer is returned. */
    HYP_FORCE_INLINE RC<UIGridColumn> GetColumn(SizeType index) const
        { return index < m_columns.Size() ? m_columns[index] : nullptr; }

    /*! \brief Finds the first empty column in the row.
        * 
        * \return A reference counted pointer to the first empty column in the row.
        *  If no empty column is found, a null pointer is returned. */
    RC<UIGridColumn> FindEmptyColumn() const;

    virtual void Init() override;

    void UpdateLayout();
    
    virtual void UpdateSize(bool update_children = true) override;
    
private:
    Array<RC<UIGridColumn>> m_columns;
};

#pragma endregion UIGridRow

#pragma region UIGrid

HYP_CLASS()
class HYP_API UIGrid : public UIPanel
{
    HYP_OBJECT_BODY(UIGrid);

public:
    UIGrid(UIStage *stage, NodeProxy node_proxy);
    UIGrid(const UIGrid &other)                 = delete;
    UIGrid &operator=(const UIGrid &other)      = delete;
    UIGrid(UIGrid &&other) noexcept             = delete;
    UIGrid &operator=(UIGrid &&other) noexcept  = delete;
    virtual ~UIGrid() override                  = default;

    /*! \brief Gets the number of columns in the grid.
     * 
     * \return The number of columns in the grid. */
    HYP_FORCE_INLINE SizeType GetNumColumns() const
        { return m_num_columns; }

    /*! \brief Sets the number of columns in the grid.
     * 
     * \param num_columns The number of columns to set. */
    void SetNumColumns(SizeType num_columns);

    /*! \brief Gets the number of rows in the grid.
     * 
     * \return The number of rows in the grid. */
    HYP_FORCE_INLINE SizeType GetNumRows() const
        { return m_rows.Size(); }

    /*! \brief Sets the number of rows in the grid.
     * 
     * \param num_rows The number of rows to set. */
    void SetNumRows(SizeType num_rows);

    RC<UIGridRow> AddRow();

    virtual void Init() override;

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    virtual void UpdateSize(bool update_children = true) override;

protected:
    virtual void SetDataSource_Internal(UIDataSourceBase *data_source) override;

private:
    void UpdateLayout();

    SizeType                m_num_columns;

    RC<UIPanel>             m_container;

    Array<RC<UIGridRow>>    m_rows;
};

#pragma endregion UIGrid

} // namespace hyperion

#endif