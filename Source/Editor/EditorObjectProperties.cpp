/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorObjectProperties.hpp>

#include <UI/UIStage.hpp>
#include <UI/UIObject.hpp>
#include <UI/UIPanel.hpp>
#include <UI/UIGrid.hpp>
#include <UI/UITextbox.hpp>

#include <Core/reflection/ClassRegistry.hpp>

namespace Hyperion {

#pragma region EditorObjectPropertiesBase

EditorObjectPropertiesBase::EditorObjectPropertiesBase(TypeId typeId)
    : m_typeId(typeId)
{
}

const Class* EditorObjectPropertiesBase::GetClass() const
{
    return ClassRegistry::GetInstance().GetClass(m_typeId);
}

#pragma endregion EditorObjectPropertiesBase

#pragma region EditorObjectProperties < Vec2f>

Handle<UIObject> EditorObjectProperties<Vec2f>::CreateUIObject(UIObject* parent) const
{
    Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    Handle<UIGridRow> row = grid->AddRow();

    // testing

    {
        Handle<UIGridColumn> col = row->AddColumn();

        Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 35, UIObjectSize::PIXEL }));
        textbox->SetText("0.00000"); // temp, just for testing
        panel->AddChildUIObject(textbox);

        col->AddChildUIObject(panel);
    }

    {
        Handle<UIGridColumn> col = row->AddColumn();

        Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 35, UIObjectSize::PIXEL }));
        textbox->SetText("0.00000"); // temp, just for
        panel->AddChildUIObject(textbox);

        col->AddChildUIObject(panel);
    }

    return grid;
}

#pragma endregion EditorObjectProperties < Vec2f>

} // namespace Hyperion
