/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>
#include <ui/UIDataSource.hpp>

#include <input/InputManager.hpp>

#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

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

void UIGridRow::SetNumColumns(SizeType num_columns)
{
    if (GetStage() == nullptr) {
        return;
    }

    const SizeType current_num_columns = m_columns.Size();

    if (num_columns == current_num_columns) {
        return;
    }

    if (num_columns < current_num_columns) {
        for (SizeType i = num_columns; i < current_num_columns; i++) {
            UIObject::RemoveChildUIObject(m_columns[i]);
        }

        m_columns.Resize(num_columns);
    } else {
        const SizeType num_columns_to_add = num_columns - current_num_columns;

        for (SizeType i = 0; i < num_columns_to_add; i++) {
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

    for (SizeType i = 0; i < m_columns.Size(); i++) {
        const RC<UIGridColumn> &column = m_columns[i];

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

void UIGrid::SetNumColumns(SizeType num_columns)
{
    m_num_columns = num_columns;

    for (const RC<UIGridRow> &row : m_rows) {
        if (!row) {
            continue;
        }

        row->SetNumColumns(num_columns);
    }

    UpdateLayout();
}

void UIGrid::SetNumRows(SizeType num_rows)
{
    if (GetStage() == nullptr) {
        return;
    }

    const SizeType current_num_rows = m_rows.Size();

    if (num_rows == current_num_rows) {
        return;
    }

    if (num_rows < current_num_rows) {
        for (SizeType i = num_rows; i < current_num_rows; i++) {
            UIObject::RemoveChildUIObject(m_rows[i]);
        }

        m_rows.Resize(num_rows);
    } else {
        const SizeType num_rows_to_add = num_rows - current_num_rows;

        for (SizeType i = 0; i < num_rows_to_add; i++) {
            RC<UIGridRow> row = GetStage()->CreateUIObject<UIGridRow>(NAME("Row"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
            UIObject::AddChildUIObject(row);

            m_rows.PushBack(std::move(row));
        }
    }

    UpdateLayout();
}

RC<UIGridRow> UIGrid::AddRow()
{
    AssertThrow(GetStage() != nullptr);

    const RC<UIGridRow> row = GetStage()->CreateUIObject<UIGridRow>(NAME("Row"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
    UIObject::AddChildUIObject(row);

    m_rows.PushBack(row);

    UpdateLayout();

    AssertThrow(row->GetStage() != nullptr);

    return row;
}

void UIGrid::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();
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
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }
    
    for (SizeType i = 0; i < m_rows.Size(); i++) {
        if (m_rows[i] == ui_object) {
            AssertThrow(UIObject::RemoveChildUIObject(ui_object));

            m_rows.EraseAt(i);

            // Updates layout as well
            UpdateSize(false);

            return true;
        }
    }

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

    for (SizeType i = 0; i < m_rows.Size(); i++) {
        RC<UIGridRow> &row = m_rows[i];

        if (!row) {
            continue;
        }

        row->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { MathUtil::Floor<float, int>(100.0f * row_percent), UIObjectSize::PERCENT }));
        row->SetPosition({ 0, y_offset });

        y_offset += MathUtil::Floor<float, int>(float(actual_size.y) * row_percent);
    }
}

void UIGrid::SetDataSource_Internal(UIDataSourceBase *data_source)
{
    RemoveAllChildUIObjects();

    if (!data_source) {
        return;
    }

    m_data_source_on_element_add_handler = data_source->OnElementAdd.Bind([this, data_source](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Add element from data source to grid view");

        // add new row
        RC<UIGridRow> row = AddRow();
        AssertThrow(row != nullptr);
        
        row->SetDataSourceElementUUID(element->GetUUID());

        // add new columns
        for (SizeType i = 0; i < m_num_columns; i++) {
            AssertThrow(GetStage() != nullptr);

            RC<UIGridColumn> column = row->AddColumn();
            AssertThrowMsg(column != nullptr, "Failed to add column to row : %llu\t%p", i, column.GetRefCountData_Internal());

            RC<UIObject> object = data_source_ptr->GetElementFactory()->CreateUIObject(GetStage(), element->GetValue());
            AssertThrow(object != nullptr);

            column->AddChildUIObject(object);
        }
    });

    m_data_source_on_element_remove_handler = data_source->OnElementRemove.Bind([this](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Remove element from data source from list view");

        const auto it = m_rows.FindIf([uuid = element->GetUUID()](const RC<UIGridRow> &row)
        {
            if (!row) {
                return false;
            }

            return row->GetDataSourceElementUUID() == uuid;
        });

        if (it != m_rows.End()) {            
            RemoveChildUIObject(*it);
        }
    });

    m_data_source_on_element_update_handler = data_source->OnElementUpdate.Bind([this, data_source](UIDataSourceBase *data_source_ptr, IUIDataSourceElement *element)
    {
        HYP_NAMED_SCOPE("Update element from data source in grid view");

        const auto it = m_rows.FindIf([uuid = element->GetUUID()](const RC<UIGridRow> &row)
        {
            if (!row) {
                return false;
            }

            return row->GetDataSourceElementUUID() == uuid;
        });

        if (it != m_rows.End()) {
            const RC<UIGridRow> &row = *it;

            for (const RC<UIGridColumn> &column : row->GetColumns()) {
                if (!column) {
                    continue;
                }

                if (const RC<UIObject> &ui_object = column->GetChildUIObject(0)) {
                    data_source->GetElementFactory()->UpdateUIObject(
                        ui_object.Get(),
                        element->GetValue()
                    );
                } else {
                    HYP_LOG(UI, LogLevel::ERROR, "Failed to update element {}; No UIObject child at index 0", element->GetUUID().ToString());
                }
            }
        }

    });
}

#pragma region UIGrid

} // namespace hyperion
