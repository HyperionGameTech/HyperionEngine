/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <editor/ui/EditorPropertyPanel.hpp>

#include <ui/UIPanel.hpp>

#include <EditorPropertyPanel.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);
HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorPropertyPanelBase

EditorPropertyPanelBase::EditorPropertyPanelBase()
    : UIPanel()
{
    SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
}

EditorPropertyPanelBase::~EditorPropertyPanelBase()
{
}

void EditorPropertyPanelBase::Init()
{
    UIObject::Init();
}

void EditorPropertyPanelBase::UpdateSize_Internal(bool updateChildren)
{
    UIObject::UpdateSize_Internal(updateChildren);
}

MaterialParameters EditorPropertyPanelBase::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

#pragma endregion EditorPropertyPanelBase

} // namespace Hyperion
