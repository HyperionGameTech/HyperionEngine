/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <ui/UIPanel.hpp>

#include <Core/containers/Array.hpp>

namespace Hyperion {

class UIGrid;

#pragma region UIGridColumn

HYP_CLASS()
class HYP_API UIGridColumn : public UIPanel
{
    HYP_OBJECT_BODY(UIGridColumn);

public:
    UIGridColumn();
    UIGridColumn(const UIGridColumn& other) = delete;
    UIGridColumn& operator=(const UIGridColumn& other) = delete;
    UIGridColumn(UIGridColumn&& other) noexcept = delete;
    UIGridColumn& operator=(UIGridColumn&& other) noexcept = delete;
    virtual ~UIGridColumn() override = default;

    HYP_METHOD(Property = "ColumnSize", XMLAttribute = "colsize")
    HYP_FORCE_INLINE int GetColumnSize() const
    {
        return m_columnSize;
    }

    HYP_METHOD(Property = "ColumnSize", XMLAttribute = "colsize")
    HYP_FORCE_INLINE void SetColumnSize(int columnSize)
    {
        m_columnSize = columnSize;
    }

private:
    virtual void Init() override;

    int m_columnSize;
};

#pragma endregion UIGridColumn

#pragma region UIGridRow

HYP_CLASS()
class HYP_API UIGridRow : public UIPanel
{
    HYP_OBJECT_BODY(UIGridRow);

public:
    friend class UIGrid;

    UIGridRow();
    UIGridRow(const UIGridRow& other) = delete;
    UIGridRow& operator=(const UIGridRow& other) = delete;
    UIGridRow(UIGridRow&& other) noexcept = delete;
    UIGridRow& operator=(UIGridRow&& other) noexcept = delete;
    virtual ~UIGridRow() override = default;

    /*! \brief Gets the columns in the row.
     *
     * \return A array of the UIGridColumn objects in the row. */
    HYP_FORCE_INLINE const Array<UIGridColumn*>& GetColumns() const
    {
        return m_columns;
    }

    int GetNumColumns() const;
    void SetNumColumns(int numColumns);

    /*! \brief Adds a new column to the row.
     *
     * \return A reference counted pointer to the newly created column. */
    Handle<UIGridColumn> AddColumn();

    /*! \brief Gets the column at the specified index.
     *
     * \param index The index of the column to retrieve.
     * \return A reference counted pointer to the column at the specified index.
     *  If the index is out of bounds, a null pointer is returned. */
    HYP_FORCE_INLINE UIGridColumn* GetColumn(size_t index) const
    {
        return index < m_columns.Size() ? m_columns[index] : nullptr;
    }

    /*! \brief Finds the first empty column in the row.
     *
     * \return The first empty column in the row.
     *  If no empty column is found, a null pointer is returned. */
    UIGridColumn* FindEmptyColumn() const;

    void UpdateColumnSizes();
    void UpdateColumnOffsets();

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

private:
    virtual void Init() override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    int m_numColumns;

    Array<UIGridColumn*> m_columns;
};

#pragma endregion UIGridRow

#pragma region UIGrid

HYP_CLASS()
class HYP_API UIGrid : public UIPanel
{
    HYP_OBJECT_BODY(UIGrid);

public:
    UIGrid();
    UIGrid(const UIGrid& other) = delete;
    UIGrid& operator=(const UIGrid& other) = delete;
    UIGrid(UIGrid&& other) noexcept = delete;
    UIGrid& operator=(UIGrid&& other) noexcept = delete;
    virtual ~UIGrid() override = default;

    /*! \brief Gets the number of columns in the grid.
     *
     * \return The number of columns in the grid. */
    HYP_METHOD(Property = "NumColumns")
    HYP_FORCE_INLINE int GetNumColumns() const
    {
        return m_numColumns;
    }

    /*! \brief Sets the number of columns in the grid.
     *
     * \param numColumns The number of columns to set. */
    HYP_METHOD(Property = "NumColumns", XMLAttribute = "cols")
    void SetNumColumns(int numColumns);

    /*! \brief Gets the number of rows in the grid.
     *
     * \return The number of rows in the grid. */
    HYP_METHOD(Property = "NumRows")
    HYP_FORCE_INLINE uint32 GetNumRows() const
    {
        return m_rows.Size();
    }

    /*! \brief Sets the number of rows in the grid.
     *
     * \param numRows The number of rows to set. */
    HYP_METHOD(Property = "NumRows", XMLAttribute = "rows")
    void SetNumRows(uint32 numRows);

    Handle<UIGridRow> AddRow();

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

protected:
    virtual void Init() override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    virtual void SetDataSource_Internal(UIDataSourceBase* dataSource) override;

private:
    void UpdateLayout();

    int m_numColumns;

    Array<UIGridRow*> m_rows;
};

#pragma endregion UIGrid

} // namespace Hyperion
