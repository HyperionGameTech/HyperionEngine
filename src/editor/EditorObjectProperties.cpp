/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorObjectProperties.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIObject.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UITextbox.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

#pragma region EditorObjectPropertiesBase

EditorObjectPropertiesBase::EditorObjectPropertiesBase(TypeID type_id)
    : m_type_id(type_id)
{
}

const HypClass* EditorObjectPropertiesBase::GetClass() const
{
    return HypClassRegistry::GetInstance().GetClass(m_type_id);
}

#pragma endregion EditorObjectPropertiesBase

#pragma region EditorObjectProperties<Vec2f>

RC<UIObject> EditorObjectProperties<Vec2f>::CreateUIObject(UIObject* parent) const
{
    RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    RC<UIGridRow> row = grid->AddRow();

    // testing

    {
        RC<UIGridColumn> col = row->AddColumn();

        RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 35, UIObjectSize::PIXEL }));
        textbox->SetText("0.00000"); // temp, just for testing
        panel->AddChildUIObject(textbox);

        col->AddChildUIObject(panel);
    }

    {
        RC<UIGridColumn> col = row->AddColumn();

        RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 35, UIObjectSize::PIXEL }));
        textbox->SetText("0.00000"); // temp, just for
        panel->AddChildUIObject(textbox);

        col->AddChildUIObject(panel);
    }

    return grid;
}

#pragma endregion EditorObjectProperties < Vec2f>

} // namespace hyperion
