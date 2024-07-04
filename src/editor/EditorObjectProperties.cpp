/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorObjectProperties.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIObject.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UIGrid.hpp>

#include <core/HypClass.hpp>
#include <core/HypClassRegistry.hpp>

namespace hyperion {

#pragma region EditorObjectPropertiesBase

EditorObjectPropertiesBase::EditorObjectPropertiesBase(TypeID type_id)
    : m_type_id(type_id)
{
}

const HypClass *EditorObjectPropertiesBase::GetClass() const
{
    return HypClassRegistry::GetInstance().GetClass(m_type_id);
}

#pragma endregion EditorObjectPropertiesBase

#pragma region EditorObjectProperties<Vec2f>

RC<UIObject> EditorObjectProperties<Vec2f>::CreateUIObject(UIStage *stage) const
{
    RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    grid->SetNumColumns(2);

    RC<UIGridRow> row = grid->AddRow();

    // testing

    {
        RC<UIGridColumn> col = row->AddColumn();

        RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
        panel->SetBackgroundColor(Vec4f::One());
        col->AddChildUIObject(panel);
    }

    {
        RC<UIGridColumn> col = row->AddColumn();

        RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(Name::Unique(), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
        panel->SetBackgroundColor(Vec4f(1, 0, 0, 1));
        col->AddChildUIObject(panel);
    }

    return grid;
}

#pragma endregion EditorObjectProperties<Vec2f>

} // namespace hyperion
