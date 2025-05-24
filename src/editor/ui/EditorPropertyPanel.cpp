/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/ui/EditorPropertyPanel.hpp>

#include <ui/UIPanel.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

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

void EditorPropertyPanelBase::UpdateSize_Internal(bool update_children)
{
    UIObject::UpdateSize_Internal(update_children);
}

Material::ParameterTable EditorPropertyPanelBase::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

#pragma endregion EditorPropertyPanelBase

} // namespace hyperion
