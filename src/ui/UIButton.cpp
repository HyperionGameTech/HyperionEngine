/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

UIButton::UIButton()
    : UIObject(UIObjectType::BUTTON)
{
    SetBorderRadius(5);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding({ 10, 5 });
    SetBackgroundColor(Vec4f { 0.25f, 0.25f, 0.25f, 1.0f });
    SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    SetTextSize(12.0f);
}

void UIButton::Init()
{
    UIObject::Init();

    RC<UIText> text_element = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    text_element->SetParentAlignment(UIObjectAlignment::CENTER);
    text_element->SetOriginAlignment(UIObjectAlignment::CENTER);
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
}

void UIButton::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    UIObject::SetFocusState_Internal(focus_state);

    UpdateMaterial(false);
    UpdateMeshData();
}

Material::ParameterTable UIButton::GetMaterialParameters() const
{
    Color color;

    if (GetFocusState() & UIObjectFocusState::PRESSED) {
        color = Vec4f(0.35f, 0.35f, 0.35f, 1.0f);
    } else if (GetFocusState() & UIObjectFocusState::HOVER) {
        color = Vec4f(0.5f, 0.5f, 0.5f, 1.0f);
    } else {
        color = m_background_color;
    }

    return Material::ParameterTable {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(color) }
    };
}

} // namespace hyperion
