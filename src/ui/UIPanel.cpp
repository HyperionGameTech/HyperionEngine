/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIPanel.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UIPanel::UIPanel(UIStage *parent, NodeProxy node_proxy, UIObjectType type)
    : UIObject(parent, std::move(node_proxy), type)
{
    SetBorderRadius(0);

    m_background_color = Color(0x101012FFu);
    m_text_color = Color(0xFFFFFFFFu);

    OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
    {
        if (GetActualInnerSize().x <= GetActualSize().x && GetActualInnerSize().y <= GetActualSize().y) {
            // allow parent to scroll
            return UIEventHandlerResult::OK;
        }

        SetScrollOffset(GetScrollOffset() - event_data.wheel * 10, /* smooth */ true);

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();
}

UIPanel::UIPanel(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::PANEL)
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
