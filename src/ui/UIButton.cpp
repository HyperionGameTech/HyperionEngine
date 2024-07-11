/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

UIButton::UIButton(UIStage *parent, NodeProxy node_proxy)
    : UIObject(parent, std::move(node_proxy), UIObjectType::BUTTON)
{
    SetBorderRadius(5);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding({ 10, 5 });
    SetBackgroundColor(Vec4f { 0.2f, 0.2f, 0.2f, 1.0f });
}

void UIButton::Init()
{
    UIObject::Init();

    RC<UIText> text_element = GetStage()->CreateUIObject<UIText>(NAME("ButtonText"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 14, UIObjectSize::PIXEL }));
    text_element->SetParentAlignment(UIObjectAlignment::CENTER);
    text_element->SetOriginAlignment(UIObjectAlignment::CENTER);
    text_element->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    text_element->SetText(m_text);

    m_text_element = text_element;

    AddChildUIObject(text_element);
}

void UIButton::SetText(const String &text)
{
    UIObject::SetText(text);

    if (m_text_element != nullptr) {
        m_text_element->SetText(m_text);
    }

    // If the size is set to AUTO, we need to update the size of the button as the text changes.
    if (m_size.GetAllFlags() & UIObjectSize::AUTO) {
        UpdateSize();
    }

    HYP_LOG(UI, LogLevel::INFO, "Button text set to: {}", m_text);
}

void UIButton::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    UIObject::SetFocusState_Internal(focus_state);

    UpdateMaterial(false);
    UpdateMeshData();

    HYP_LOG(UI, LogLevel::INFO, "Button focus state set to: {}", uint(focus_state));
}

MaterialAttributes UIButton::GetMaterialAttributes() const
{
    return MaterialAttributes {
        .shader_definition  = ShaderDefinition { NAME("UIObject"), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_BUTTON" }) },
        .bucket             = Bucket::BUCKET_UI,
        .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
        .cull_faces         = FaceCullMode::BACK,
        .flags              = MaterialAttributeFlags::NONE
    };
}

} // namespace hyperion
