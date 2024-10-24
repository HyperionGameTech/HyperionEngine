/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIPanel.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UIPanel::UIPanel(UIObjectType type)
    : UIObject(type)
{
    SetBorderRadius(0);
    SetBackgroundColor(Color(0x101012FFu));
    SetTextColor(Color(0xFFFFFFFFu));

    OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
    {
        // @FIXME Make work with Cropped AABB

        if (GetActualInnerSize().x <= GetActualSize().x && GetActualInnerSize().y <= GetActualSize().y) {
            // allow parent to scroll
            return UIEventHandlerResult::OK;
        }

        SetScrollOffset(GetScrollOffset() - event_data.wheel * 10, /* smooth */ true);

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();
}

UIPanel::UIPanel()
    : UIPanel(UIObjectType::PANEL)
{
}

void UIPanel::Init()
{
    UIObject::Init();
}

MaterialAttributes UIPanel::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

Material::ParameterTable UIPanel::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

Material::TextureSet UIPanel::GetMaterialTextures() const
{
    return UIObject::GetMaterialTextures();
}

} // namespace hyperion
