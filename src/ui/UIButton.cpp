/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <UIButton.generated.inl>

namespace hyperion {

UIButton::UIButton()
{
    SetBorderRadius(8);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding({ 10, 5 });
    SetBackgroundColor(Vec4f { 0.25f, 0.25f, 0.25f, 1.0f });
    SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    SetTextSize(12.0f);
}

void UIButton::Init()
{
    UIObject::Init();

    Handle<UIText> textElement = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    textElement->SetParentAlignment(UIObjectAlignment::CENTER);
    textElement->SetOriginAlignment(UIObjectAlignment::CENTER);
    textElement->SetText(m_text);

    m_textElement = textElement;

    AddChildUIObject(textElement);
}

void UIButton::SetText(const String& text)
{
    UIObject::SetText(text);

    if (m_textElement != nullptr)
    {
        m_textElement->SetText(m_text);
    }

    // If the size is set to AUTO, we need to update the size of the button as the text changes.
    if (m_size.GetAllFlags() & UIObjectSize::AUTO)
    {
        UpdateSize();
    }
}

void UIButton::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState)
{
    UIObject::SetFocusState_Internal(focusState);

    UpdateMaterial(false);
    UpdateMeshData();
}

MaterialParameters UIButton::GetMaterialParameters() const
{
    Color color;

    if (GetFocusState() & UIObjectFocusState::PRESSED)
    {
        color = Vec4f(0.35f, 0.35f, 0.35f, 1.0f);
    }
    else if (GetFocusState() & UIObjectFocusState::HOVER)
    {
        color = Vec4f(0.5f, 0.5f, 0.5f, 1.0f);
    }
    else
    {
        color = m_backgroundColor;
    }

    return MaterialParameters {
        { MATERIAL_KEY_ALBEDO, Vec4f(color) }
    };
}

} // namespace hyperion
