/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/ui/EditorPropertyPanel.hpp>

#include <ui/UIPanel.hpp>

#include <core/logging/Logger.hpp>

#include <EditorPropertyPanel.generated.inl>

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

void EditorPropertyPanelBase::UpdateSize_Internal(bool updateChildren)
{
    UIObject::UpdateSize_Internal(updateChildren);
}

MaterialParameters EditorPropertyPanelBase::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

#pragma endregion EditorPropertyPanelBase

} // namespace hyperion
