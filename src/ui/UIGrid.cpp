/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>

#include <input/InputManager.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region UIGridColumn

UIGridColumn::UIGridColumn(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::GRID_COLUMN)
{
}

void UIGridColumn::Init()
{
    UIPanel::Init();
}

#pragma endregion UIGridColumn

#pragma region UIGridRow

UIGridRow::UIGridRow(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::GRID_ROW)
{
}

void UIGridRow::Init()
{
    UIPanel::Init();
}

RC<UIGridColumn> UIGridRow::FindEmptyColumn() const
{
    for (const RC<UIGridColumn> &column : m_columns) {
        if (!column) {
            continue;
        }

        if (!column->HasChildUIObjects()) {
            return column;
        }
    }

    return nullptr;
}

void UIGridRow::SetNumColumns(uint num_columns)
{
    if (GetStage() == nullptr) {
        return;
    }

    const uint current_num_columns = m_columns.Size();

    if (num_columns == current_num_columns) {
        return;
    }

    if (num_columns < current_num_columns) {
        for (uint i = num_columns; i < current_num_columns; i++) {
            UIObject::RemoveChildUIObject(m_columns[i]);
        }

        m_columns.Resize(num_columns);
    } else {
        const uint num_columns_to_add = num_columns - current_num_columns;

        for (uint i = 0; i < num_columns_to_add; i++) {
            RC<UIGridColumn> column = GetStage()->CreateUIObject<UIGridColumn>(NAME("Column"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
            UIObject::AddChildUIObject(column);

            m_columns.PushBack(std::move(column));
        }
    }

    UpdateLayout();
}

RC<UIGridColumn> UIGridRow::AddColumn()
{
    if (GetStage() == nullptr) {
        return nullptr;
    }

    const RC<UIGridColumn> column = GetStage()->CreateUIObject<UIGridColumn>(NAME("Column"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    UIObject::AddChildUIObject(column);

    m_columns.PushBack(column);

    UpdateLayout();

    return column;
}

void UIGridRow::UpdateLayout()
{
    const Vec2i actual_size = GetActualSize();

    const float column_percent = 1.0f / float(m_columns.Size());

    Vec2i offset { 0, 0 };

    for (uint i = 0; i < m_columns.Size(); i++) {
        RC<UIGridColumn> &column = m_columns[i];

        if (!column) {
            continue;
        }

        column->SetSize(UIObjectSize({ MathUtil::Floor<float, int>(100.0f * column_percent), UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
        column->SetPosition(offset);

        offset.x += MathUtil::Floor<float, int>(float(actual_size.x) * column_percent);
    }
}

void UIGridRow::UpdateSize(bool update_children)
{
    UIPanel::UpdateSize(update_children);

    UpdateLayout();
}

#pragma endregion UIGridRow

#pragma region UIGrid

UIGrid::UIGrid(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::GRID),
      m_num_columns(12)
{
}

void UIGrid::SetNumColumns(uint num_columns)
{
    m_num_columns = num_columns;

    UpdateLayout();
}

void UIGrid::SetNumRows(uint num_rows)
{
    if (GetStage() == nullptr) {
        return;
    }

    const uint current_num_rows = m_rows.Size();

    if (num_rows == current_num_rows) {
        return;
    }

    if (num_rows < current_num_rows) {
        for (uint i = num_rows; i < current_num_rows; i++) {
            UIObject::RemoveChildUIObject(m_rows[i]);
        }

        m_rows.Resize(num_rows);
    } else {
        const uint num_rows_to_add = num_rows - current_num_rows;

        for (uint i = 0; i < num_rows_to_add; i++) {
            RC<UIGridRow> row = GetStage()->CreateUIObject<UIGridRow>(NAME("Row"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
            m_container->AddChildUIObject(row);

            m_rows.PushBack(std::move(row));
        }
    }

    UpdateLayout();
}

RC<UIGridRow> UIGrid::AddRow()
{
    if (GetStage() == nullptr || m_container == nullptr) {
        return nullptr;
    }

    const RC<UIGridRow> row = GetStage()->CreateUIObject<UIGridRow>(NAME("Row"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    m_container->AddChildUIObject(row);

    m_rows.PushBack(row);

    UpdateLayout();

    return row;
}

void UIGrid::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    AssertThrow(GetStage() != nullptr);

    m_container = GetStage()->CreateUIObject<UIPanel>(NAME("GridContents"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    UIObject::AddChildUIObject(m_container);
}

void UIGrid::AddChildUIObject(UIObject *ui_object)
{
    RC<UIGridColumn> column;

    for (const RC<UIGridRow> &row : m_rows) {
        if (!row) {
            continue;
        }

        column = row->FindEmptyColumn();

        if (column != nullptr) {
            column->AddChildUIObject(ui_object);

            return;
        }
    }

    AddRow();
    column = m_rows.Back()->FindEmptyColumn();
    column->AddChildUIObject(ui_object);
}

bool UIGrid::RemoveChildUIObject(UIObject *ui_object)
{
    return UIObject::RemoveChildUIObject(ui_object);
}

void UIGrid::UpdateSize(bool update_children)
{
    UIPanel::UpdateSize(update_children);

    UpdateLayout();
}

void UIGrid::UpdateLayout()
{
    if (m_rows.Empty()) {
        return;
    }

    int y_offset = 0;

    const Vec2i actual_size = GetActualSize();

    const float row_percent = 1.0f / float(m_rows.Size());

    for (uint i = 0; i < m_rows.Size(); i++) {
        RC<UIGridRow> &row = m_rows[i];

        if (!row) {
            continue;
        }

        row->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { MathUtil::Floor<float, int>(100.0f * row_percent), UIObjectSize::PERCENT }));
        row->SetPosition({ 0, y_offset });
        row->SetNumColumns(m_num_columns);

        y_offset += MathUtil::Floor<float, int>(float(actual_size.y) * row_percent);
    }

    // DebugLog(LogType::Debug, "Grid has %u rows. Size: %d, %d\tContainer size: %d, %d\n", m_rows.Size(), actual_size.x, actual_size.y,
    //     m_container->GetActualSize().x, m_container->GetActualSize().y);

    // for (uint i = 0; i < m_rows.Size(); i++) {
    //     RC<UIGridRow> &row = m_rows[i];

    //     if (!row) {
    //         continue;
    //     }

    //     DebugLog(LogType::Debug, "Row %u has %u columns.  Width: %d, Height: %d\n", i, row->GetNumColumns(),
    //         row->GetActualSize().x, row->GetActualSize().y);
    // }
}

#pragma region UIGrid

} // namespace hyperion
