/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>
#include <ui/UIDataSource.hpp>

#include <input/InputManager.hpp>

#include <core/logging/Logger.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIGridColumn

UIGridColumn::UIGridColumn()
    : UIPanel(UIObjectType::GRID_COLUMN),
      m_column_size(1)
{
    SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
}

void UIGridColumn::Init()
{
    UIPanel::Init();
}

#pragma endregion UIGridColumn

#pragma region UIGridRow

UIGridRow::UIGridRow()
    : UIPanel(UIObjectType::GRID_ROW),
      m_num_columns(0)
{
    SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
}

void UIGridRow::Init()
{
    UIPanel::Init();
}

void UIGridRow::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;
    
    if (ui_object.Is<UIGridColumn>()) {
        UIObject::AddChildUIObject(ui_object);

        m_columns.PushBack(ui_object->RefCountedPtrFromThis().CastUnsafe<UIGridColumn>());

        UpdateColumnSizes();
        UpdateColumnOffsets();

        return;
    }

    RC<UIGridColumn> column = FindEmptyColumn();
    
    if (column == nullptr) {
        column = AddColumn();
    }

    column->AddChildUIObject(ui_object);

    UpdateColumnSizes();
    UpdateColumnOffsets();
}

bool UIGridRow::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    const bool removed = UIObject::RemoveChildUIObject(ui_object);

    if (!removed) {
        return false;
    }

    if (IsInstanceOfHypClass<UIGridColumn>(ui_object)) {
        auto it = m_columns.FindAs(ui_object);

        if (it != m_columns.End()) {
            m_columns.Erase(it);

            UpdateColumnSizes();
            UpdateColumnOffsets();
        }
    }

    return true;
}

RC<UIGridColumn> UIGridRow::FindEmptyColumn() const
{
    HYP_SCOPE;

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

int UIGridRow::GetNumColumns() const
{
    return m_num_columns < 0 ? int(m_columns.Size()) : MathUtil::Max(int(m_columns.Size()), m_num_columns);
}

void UIGridRow::SetNumColumns(int num_columns)
{
    HYP_SCOPE;

    if (m_num_columns == num_columns) {
        return;
    }

    m_num_columns = num_columns;

    // if (m_num_columns >= 0) {
    //     if (m_columns.Size() < SizeType(m_num_columns)) {
    //         const SizeType num_columns_to_add = SizeType(m_num_columns) - m_columns.Size();

    //         for (SizeType i = 0; i < num_columns_to_add; i++) {
    //             AddColumn();
    //         }
    //     } else if (m_columns.Size() > SizeType(m_num_columns)) {
    //         const SizeType num_columns_to_remove = m_columns.Size() - SizeType(m_num_columns);

    //         for (SizeType i = 0; i < num_columns_to_remove; i++) {
    //             UIObject::RemoveChildUIObject(m_columns[m_columns.Size() - 1]);
    //         }

    //         m_columns.Resize(SizeType(m_num_columns));
    //     }
    // }

    UpdateColumnSizes();
    UpdateColumnOffsets();
}

RC<UIGridColumn> UIGridRow::AddColumn()
{
    HYP_SCOPE;

    const RC<UIGridColumn> column = CreateUIObject<UIGridColumn>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    UIObject::AddChildUIObject(column);

    m_columns.PushBack(column);

    if (m_num_columns >= 0) {
        m_num_columns = SizeType(m_columns.Size());
    }

    UpdateColumnSizes();
    UpdateColumnOffsets();

    return column;
}

void UIGridRow::UpdateColumnSizes()
{
    HYP_SCOPE;

    const float column_size_percent = 1.0f / float(GetNumColumns());

    Vec2i offset { 0, 0 };

    for (SizeType i = 0; i < m_columns.Size(); i++) {
        const RC<UIGridColumn> &column = m_columns[i];

        if (!column) {
            continue;
        }

        const UIObjectSize current_column_size = column->GetSize();

        column->SetSize(UIObjectSize({ MathUtil::Floor<float, int>(100.0f * float(column->GetColumnSize()) * column_size_percent), UIObjectSize::PERCENT }, { current_column_size.GetValue().y, current_column_size.GetFlagsY() }));
    }
}

void UIGridRow::UpdateColumnOffsets()
{
    HYP_SCOPE;

    Vec2i offset { 0, 0 };

    for (SizeType i = 0; i < m_columns.Size(); i++) {
        const RC<UIGridColumn> &column = m_columns[i];

        if (!column) {
            continue;
        }

        column->SetPosition(offset);

        offset.x += column->GetActualSize().x;
    }
}

void UIGridRow::UpdateSize_Internal(bool update_children)
{
    UIPanel::UpdateSize_Internal(update_children);

    UpdateColumnOffsets();
}

#pragma endregion UIGridRow

#pragma region UIGrid

UIGrid::UIGrid()
    : UIPanel(UIObjectType::GRID),
      m_num_columns(-1)
{
}

void UIGrid::SetNumColumns(int num_columns)
{
    m_num_columns = num_columns;

    ForEachChildUIObject_Proc([this](UIObject *ui_object)
    {
        if (!ui_object->IsInstanceOf<UIGridRow>()) {
            return IterationResult::CONTINUE;
        }

        static_cast<UIGridRow *>(ui_object)->SetNumColumns(m_num_columns);

        return IterationResult::CONTINUE;
    }, false);
}

void UIGrid::SetNumRows(uint32 num_rows)
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
            RC<UIGridRow> row = CreateUIObject<UIGridRow>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            UIObject::AddChildUIObject(row);

            m_rows.PushBack(std::move(row));
        }
    }

    UpdateLayout();
}

RC<UIGridRow> UIGrid::AddRow()
{
    const RC<UIGridRow> row = CreateUIObject<UIGridRow>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    row->SetNumColumns(m_num_columns);

    if (m_num_columns >= 0) {
        for (SizeType i = 0; i < SizeType(m_num_columns); i++) {
            row->AddColumn();
        }
    }
    
    UIObject::AddChildUIObject(row);

    m_rows.PushBack(row);

    UpdateLayout();

    return row;
}

void UIGrid::Init()
{
    Threads::AssertOnThread(g_game_thread);

    UIPanel::Init();
}

void UIGrid::AddChildUIObject(const RC<UIObject> &ui_object)
{
    if (ui_object->IsInstanceOf<UIGridRow>()) {
        RC<UIGridRow> row = ui_object->RefCountedPtrFromThis().CastUnsafe<UIGridRow>();
        row->SetNumColumns(m_num_columns);

        UIObject::AddChildUIObject(row);

        m_rows.PushBack(row);

        UpdateLayout();

        return;
    }

    RC<UIGridColumn> column;

    for (const RC<UIGridRow> &row : m_rows) {
        if (!row) {
            continue;
        }

        column = row->FindEmptyColumn();

        if (column != nullptr) {
            column->AddChildUIObject(ui_object);

            UpdateLayout();

            return;
        }
    }

    if (!column) {
        RC<UIGridRow> row = AddRow();
        if (!(column = row->FindEmptyColumn())) {
            column = row->AddColumn();
        }
    }

    column->AddChildUIObject(ui_object);

    UpdateLayout();
}

bool UIGrid::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }
    
    if (ui_object->IsInstanceOf<UIGridRow>()) {
        bool removed = false;

        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        for (auto it = m_rows.Begin(); it != m_rows.End();) {
            if (it->Get() == static_cast<UIGridRow *>(ui_object)) {
                AssertThrow(UIObject::RemoveChildUIObject(ui_object));

                it = m_rows.Erase(it);

                removed = true;
            } else {
                ++it;
            }
        }

        if (removed) {
            UpdateSize(false);
        }
    }

    return UIObject::RemoveChildUIObject(ui_object);
}

void UIGrid::UpdateSize_Internal(bool update_children)
{
    UIPanel::UpdateSize_Internal(update_children);

    UpdateLayout();
}

void UIGrid::UpdateLayout()
{
    if (m_rows.Empty()) {
        return;
    }

    int y_offset = 0;

    for (SizeType i = 0; i < m_rows.Size(); i++) {
        RC<UIGridRow> &row = m_rows[i];

        if (!row) {
            continue;
        }

        row->SetPosition({ 0, y_offset });

        y_offset += row->GetActualSize().y;
    }
}

void UIGrid::SetDataSource_Internal(UIDataSourceBase *data_source)
{
    RemoveAllChildUIObjects();

    if (!data_source) {
        return;
    }

    m_data_source_on_element_add_handler = data_source->OnElementAdd.Bind([this, data_source](UIDataSourceBase *data_source_ptr, UIDataSourceElement *element, UIDataSourceElement *parent)
    {
        HYP_NAMED_SCOPE("Add element from data source to grid view");

        AddChildUIObject(data_source_ptr->CreateUIObject(this, element->GetValue(), { }));
    });

    m_data_source_on_element_remove_handler = data_source->OnElementRemove.Bind([this](UIDataSourceBase *data_source_ptr, UIDataSourceElement *element, UIDataSourceElement *parent)
    {
        HYP_NAMED_SCOPE("Remove element from data source from grid view");

        if (RC<UIObject> ui_object = FindChildUIObject([element](UIObject *ui_object) { return ui_object->GetDataSourceElementUUID() == element->GetUUID(); })) {
            RemoveChildUIObject(ui_object);
        }

        // @TODO: Rebuild grid layout so that there are no empty rows/columns
    });

    m_data_source_on_element_update_handler = data_source->OnElementUpdate.Bind([this, data_source](UIDataSourceBase *data_source_ptr, UIDataSourceElement *element, UIDataSourceElement *parent)
    {
        HYP_NAMED_SCOPE("Update element from data source in grid view");
    });
}

#pragma region UIGrid

} // namespace hyperion
