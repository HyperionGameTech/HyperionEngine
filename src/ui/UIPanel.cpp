/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIPanel.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

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
        SetScrollOffset(GetScrollOffset() - event_data.wheel * 5);

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
    return MaterialAttributes {
        .shader_definition  = ShaderDefinition { NAME("UIObject"), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_PANEL" }) },
        .bucket             = Bucket::BUCKET_UI,
        .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
        .cull_faces         = FaceCullMode::BACK,
        .flags              = MaterialAttributeFlags::NONE,
        //.stencil_function   = StencilFunction(StencilOp::KEEP, StencilOp::REPLACE, StencilOp::REPLACE, StencilCompareOp::ALWAYS, 0xFF, 0x1)// <-- @TEMP test
    };
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
