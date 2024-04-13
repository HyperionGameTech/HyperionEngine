#ifndef HYPERION_V2_UI_GRID_HPP
#define HYPERION_V2_UI_GRID_HPP

#include <ui/UIPanel.hpp>

#include <core/lib/DynArray.hpp>

namespace hyperion::v2 {

#pragma region UIGridColumn

class HYP_API UIGridColumn : public UIPanel
{
public:
    UIGridColumn(ID<Entity> entity, UIScene *ui_scene, NodeProxy node_proxy);
    UIGridColumn(const UIGridColumn &other)                 = delete;
    UIGridColumn &operator=(const UIGridColumn &other)      = delete;
    UIGridColumn(UIGridColumn &&other) noexcept             = delete;
    UIGridColumn &operator=(UIGridColumn &&other) noexcept  = delete;
    virtual ~UIGridColumn() override                        = default;

    virtual void Init() override;
};

#pragma endregion UIGridColumn

#pragma region UIGridRow

class HYP_API UIGridRow : public UIPanel
{
public:
    UIGridRow(ID<Entity> entity, UIScene *ui_scene, NodeProxy node_proxy);
    UIGridRow(const UIGridRow &other)                   = delete;
    UIGridRow &operator=(const UIGridRow &other)        = delete;
    UIGridRow(UIGridRow &&other) noexcept               = delete;
    UIGridRow &operator=(UIGridRow &&other) noexcept    = delete;
    virtual ~UIGridRow() override                       = default;

    uint GetNumColumns() const
        { return m_columns.Size(); }

    void SetNumColumns(uint num_columns);
    
    RC<UIGridColumn> AddColumn();

    RC<UIGridColumn> GetColumn(uint index) const
        { return index < m_columns.Size() ? m_columns[index] : nullptr; }

    RC<UIGridColumn> FindEmptyColumn() const;

    virtual void Init() override;

    void UpdateLayout();
    
    virtual void UpdateSize() override;
    
private:
    Array<RC<UIGridColumn>> m_columns;
};

#pragma endregion UIGridRow

#pragma region UIGrid

class HYP_API UIGrid : public UIPanel
{
public:
    UIGrid(ID<Entity> entity, UIScene *ui_scene, NodeProxy node_proxy);
    UIGrid(const UIGrid &other)                 = delete;
    UIGrid &operator=(const UIGrid &other)      = delete;
    UIGrid(UIGrid &&other) noexcept             = delete;
    UIGrid &operator=(UIGrid &&other) noexcept  = delete;
    virtual ~UIGrid() override                  = default;

    uint GetNumColumns() const
        { return m_num_columns; }

    void SetNumColumns(uint num_columns);

    uint GetNumRows() const
        { return m_rows.Size(); }

    void SetNumRows(uint num_rows);

    RC<UIGridRow> AddRow();

    virtual void Init() override;

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    virtual void UpdateSize() override;

private:
    void UpdateLayout();

    uint                    m_num_columns;

    RC<UIPanel>             m_container;

    Array<RC<UIGridRow>>    m_rows;
};

#pragma endregion UIGrid

} // namespace hyperion::v2

#endif