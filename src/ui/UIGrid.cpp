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
    : m_columnSize(1)
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
    : m_numColumns(0)
{
    SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
}

void UIGridRow::Init()
{
    UIPanel::Init();
}

void UIGridRow::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;

    if (uiObject.IsValid() && uiObject->IsA<UIGridColumn>())
    {
        UIObject::AddChildUIObject(uiObject);

        m_columns.PushBack(ObjCast<UIGridColumn>(uiObject));

        UpdateColumnSizes();
        UpdateColumnOffsets();

        return;
    }

    UIGridColumn* column = FindEmptyColumn();

    if (column == nullptr)
    {
        column = AddColumn();
    }

    column->AddChildUIObject(uiObject);

    UpdateColumnSizes();
    UpdateColumnOffsets();
}

bool UIGridRow::RemoveChildUIObject(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return false;
    }

    // Keep a reference to the UIObject before removing it
    Handle<UIObject> uiObjectHandle = MakeStrongRef(uiObject);

    if (!UIObject::RemoveChildUIObject(uiObject))
    {
        return false;
    }

    if (uiObjectHandle->IsA<UIGridColumn>())
    {
        auto it = m_columns.FindAs(uiObject);

        if (it != m_columns.End())
        {
            m_columns.Erase(it);

            UpdateColumnSizes();
            UpdateColumnOffsets();
        }
    }

    return true;
}

UIGridColumn* UIGridRow::FindEmptyColumn() const
{
    HYP_SCOPE;

    for (UIGridColumn* column : m_columns)
    {
        if (!column)
        {
            continue;
        }

        if (!column->HasChildUIObjects())
        {
            return column;
        }
    }

    return nullptr;
}

int UIGridRow::GetNumColumns() const
{
    return m_numColumns < 0 ? int(m_columns.Size()) : MathUtil::Max(int(m_columns.Size()), m_numColumns);
}

void UIGridRow::SetNumColumns(int numColumns)
{
    HYP_SCOPE;

    if (m_numColumns == numColumns)
    {
        return;
    }

    m_numColumns = numColumns;

    // if (m_numColumns >= 0) {
    //     if (m_columns.Size() < SizeType(m_numColumns)) {
    //         const SizeType numColumnsToAdd = SizeType(m_numColumns) - m_columns.Size();

    //         for (SizeType i = 0; i < numColumnsToAdd; i++) {
    //             AddColumn();
    //         }
    //     } else if (m_columns.Size() > SizeType(m_numColumns)) {
    //         const SizeType numColumnsToRemove = m_columns.Size() - SizeType(m_numColumns);

    //         for (SizeType i = 0; i < numColumnsToRemove; i++) {
    //             UIObject::RemoveChildUIObject(m_columns[m_columns.Size() - 1]);
    //         }

    //         m_columns.Resize(SizeType(m_numColumns));
    //     }
    // }

    UpdateColumnSizes();
    UpdateColumnOffsets();
}

Handle<UIGridColumn> UIGridRow::AddColumn()
{
    HYP_SCOPE;

    const Handle<UIGridColumn> column = CreateUIObject<UIGridColumn>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    UIObject::AddChildUIObject(column);

    m_columns.PushBack(column);

    if (m_numColumns >= 0)
    {
        m_numColumns = SizeType(m_columns.Size());
    }

    UpdateColumnSizes();
    UpdateColumnOffsets();

    return column;
}

void UIGridRow::UpdateColumnSizes()
{
    HYP_SCOPE;

    const float columnSizePercent = 1.0f / float(GetNumColumns());

    Vec2i offset { 0, 0 };

    for (SizeType i = 0; i < m_columns.Size(); i++)
    {
        UIGridColumn* column = m_columns[i];

        if (!column)
        {
            continue;
        }

        const UIObjectSize currentColumnSize = column->GetSize();

        column->SetSize(UIObjectSize({ MathUtil::Floor<float, int>(100.0f * float(column->GetColumnSize()) * columnSizePercent), UIObjectSize::PERCENT }, { currentColumnSize.GetValue().y, currentColumnSize.GetFlagsY() }));
    }
}

void UIGridRow::UpdateColumnOffsets()
{
    HYP_SCOPE;

    Vec2i offset { 0, 0 };

    for (SizeType i = 0; i < m_columns.Size(); i++)
    {
        UIGridColumn* column = m_columns[i];

        if (!column)
        {
            continue;
        }

        column->SetPosition(offset);

        offset.x += column->GetActualSize().x;
    }
}

void UIGridRow::UpdateSize_Internal(bool updateChildren)
{
    UIPanel::UpdateSize_Internal(updateChildren);

    UpdateColumnOffsets();

    if (UseAutoSizing())
    {
        // If the row is using auto-sizing, we need to refit to the new size
        UIPanel::UpdateSize_Internal(false);
    }
}

#pragma endregion UIGridRow

#pragma region UIGrid

UIGrid::UIGrid()
    : m_numColumns(-1)
{
}

void UIGrid::SetNumColumns(int numColumns)
{
    m_numColumns = numColumns;

    ForEachChildUIObject_Proc([this](UIObject* uiObject)
        {
            if (!uiObject->IsA<UIGridRow>())
            {
                return IterationResult::CONTINUE;
            }

            static_cast<UIGridRow*>(uiObject)->SetNumColumns(m_numColumns);

            return IterationResult::CONTINUE;
        },
        false);
}

void UIGrid::SetNumRows(uint32 numRows)
{
    const SizeType currentNumRows = m_rows.Size();

    if (numRows == currentNumRows)
    {
        return;
    }

    if (numRows < currentNumRows)
    {
        for (SizeType i = numRows; i < currentNumRows; i++)
        {
            UIObject::RemoveChildUIObject(m_rows[i]);
        }

        m_rows.Resize(numRows);
    }
    else
    {
        const SizeType numRowsToAdd = numRows - currentNumRows;

        for (SizeType i = 0; i < numRowsToAdd; i++)
        {
            Handle<UIGridRow> row = CreateUIObject<UIGridRow>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            UIObject::AddChildUIObject(row);

            m_rows.PushBack(std::move(row));
        }
    }

    UpdateLayout();
}

Handle<UIGridRow> UIGrid::AddRow()
{
    const Handle<UIGridRow> row = CreateUIObject<UIGridRow>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    row->SetNumColumns(m_numColumns);

    if (m_numColumns >= 0)
    {
        for (SizeType i = 0; i < SizeType(m_numColumns); i++)
        {
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
    Threads::AssertOnThread(g_gameThread);

    UIPanel::Init();
}

void UIGrid::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    if (!uiObject.IsValid())
    {
        return;
    }

    if (const Handle<UIGridRow>& row = ObjCast<UIGridRow>(uiObject))
    {
        row->SetNumColumns(m_numColumns);

        UIObject::AddChildUIObject(row);

        m_rows.PushBack(row);

        UpdateLayout();

        return;
    }

    UIGridColumn* column = nullptr;

    for (UIGridRow* row : m_rows)
    {
        if (!row)
        {
            continue;
        }

        column = row->FindEmptyColumn();

        if (column != nullptr)
        {
            column->AddChildUIObject(uiObject);

            UpdateLayout();

            return;
        }
    }

    if (!column)
    {
        Handle<UIGridRow> row = AddRow();

        if (!(column = row->FindEmptyColumn()))
        {
            column = row->AddColumn();
        }
    }

    column->AddChildUIObject(uiObject);

    UpdateLayout();
}

bool UIGrid::RemoveChildUIObject(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return false;
    }

    // Keep a reference around
    Handle<UIObject> uiObjectHandle = MakeStrongRef(uiObject);

    if (!UIObject::RemoveChildUIObject(uiObject))
    {
        return false;
    }

    if (uiObject->IsA<UIGridRow>())
    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        auto it = m_rows.FindAs(uiObject);

        if (it != m_rows.End())
        {
            m_rows.Erase(it);
        }

        UpdateLayout();
    }

    return true;
}

void UIGrid::UpdateSize_Internal(bool updateChildren)
{
    UIPanel::UpdateSize_Internal(updateChildren);

    UpdateLayout();

    if (UseAutoSizing())
    {
        // If the grid is using auto-sizing, we need to refit to the new size
        UIPanel::UpdateSize_Internal(false);
    }
}

void UIGrid::UpdateLayout()
{
    if (m_rows.Empty())
    {
        return;
    }

    int yOffset = 0;

    for (SizeType i = 0; i < m_rows.Size(); i++)
    {
        UIGridRow* row = m_rows[i];

        if (!row)
        {
            continue;
        }

        row->SetPosition({ 0, yOffset });

        yOffset += row->GetActualSize().y;
    }
}

void UIGrid::SetDataSource_Internal(UIDataSourceBase* dataSource)
{
    RemoveAllChildUIObjects();

    if (!dataSource)
    {
        return;
    }

    for (const auto& pair : dataSource->GetValues())
    {
        AddChildUIObject(dataSource->CreateUIObject(this, pair.second->GetValue(), {}));
    }

    m_dataSourceOnElementAddHandler = dataSource->OnElementAdd.Bind([this, dataSource](UIDataSourceBase* dataSourcePtr, UIDataSourceElement* element, UIDataSourceElement* parent)
        {
            HYP_NAMED_SCOPE("Add element from data source to grid view");

            Handle<UIObject> object = dataSourcePtr->CreateUIObject(this, element->GetValue(), {});

            if (object)
            {
                object->SetDataSourceElementUUID(element->GetUUID());

                AddChildUIObject(object);
            }
        });

    m_dataSourceOnElementRemoveHandler = dataSource->OnElementRemove.Bind([this](UIDataSourceBase* dataSourcePtr, UIDataSourceElement* element, UIDataSourceElement* parent)
        {
            HYP_NAMED_SCOPE("Remove element from data source from grid view");

            if (Handle<UIObject> uiObject = FindChildUIObject([element](UIObject* uiObject)
                    {
                        return uiObject->GetDataSourceElementUUID() == element->GetUUID();
                    }))
            {
                RemoveChildUIObject(uiObject);
            }

            // @TODO: Rebuild grid layout so that there are no empty rows/columns
        });

    m_dataSourceOnElementUpdateHandler = dataSource->OnElementUpdate.Bind([this, dataSource](UIDataSourceBase* dataSourcePtr, UIDataSourceElement* element, UIDataSourceElement* parent)
        {
            HYP_NAMED_SCOPE("Update element from data source in grid view");
        });
}

#pragma region UIGrid

} // namespace hyperion
