/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UITextbox.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UITextbox

UITextbox::UITextbox()
    : UIPanel(UIObjectType::TEXTBOX),
      m_character_index(0)
{
    SetBorderRadius(2);
    SetPadding({ 5, 2 });
    SetTextColor(Color::Black());
    SetBackgroundColor(Vec4f::One());
    
    // For now
    SetIsScrollEnabled(UIObjectScrollbarOrientation::ALL, false);

    // @TODO Rapid press via holding key

    OnKeyDown.Bind([this](const KeyboardEvent &event_data) -> UIEventHandlerResult
    {
        char key_char;

        const bool shift = event_data.input_manager->IsShiftDown();
        const bool alt = event_data.input_manager->IsAltDown();
        const bool ctrl = event_data.input_manager->IsCtrlDown();

        if (KeyCodeToChar(event_data.key_code, shift, alt, ctrl, key_char)) {
            HYP_LOG(UI, Info, "Textbox keydown: char = {}", key_char);

            const String &text = GetText();

            if (key_char == '\b') {
                SetText(text.Substr(0, text.Length() - 1));
            } else {
                SetText(text + key_char);
            }
        }

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    OnKeyUp.Bind([this](const KeyboardEvent &event_data) -> UIEventHandlerResult
    {
        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();
}

void UITextbox::Init()
{
    Threads::AssertOnThread(g_game_thread);

    UIPanel::Init();

    SetInnerSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
    // SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    m_text_element = CreateUIObject<UIText>(NAME("TextboxText"), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    m_text_element->SetTextSize(12.0f);

    UpdateTextColor();

    // m_text_element->SetAffectsParentSize(false);

    UIObject::AddChildUIObject(m_text_element);
}

void UITextbox::SetTextColor(const Color &text_color)
{
    UIPanel::SetTextColor(text_color);

    UpdateTextColor();
}

void UITextbox::SetText(const String &text)
{
    const bool was_displaying_placeholder = ShouldDisplayPlaceholder();

    UIObject::SetText(text);

    if (m_text_element) {
        const bool should_display_placeholder = ShouldDisplayPlaceholder();

        if (should_display_placeholder != was_displaying_placeholder) {
            UpdateTextColor();
        }

        m_text_element->SetText(should_display_placeholder ? m_placeholder : text);
    }
}

void UITextbox::SetPlaceholder(const String &placeholder)
{
    m_placeholder = placeholder;

    UpdateTextColor();
}

void UITextbox::Update_Internal(GameCounter::TickUnit delta)
{
    UIPanel::Update_Internal(delta);

    if (m_cursor_element != nullptr) {
        constexpr double cursor_blink_speed = 2.5;

        if (delta <= 0.167) {
            if (!m_cursor_blink_blend_var.Advance(delta * cursor_blink_speed)) {
                m_cursor_blink_blend_var.SetTarget(1.0f - m_cursor_blink_blend_var.GetTarget());
            }
        }
        
        // animate cursor
        m_cursor_element->SetBackgroundColor(Vec4f { 0, 0, 0, m_cursor_blink_blend_var.GetValue() });

        // update cursor position
        // @TODO Get pixel position of m_character_index

        // update cursor visibility
    }
}

void UITextbox::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    const EnumFlags<UIObjectFocusState> previous_focus_state = GetFocusState();

    UIPanel::SetFocusState_Internal(focus_state);

    if ((previous_focus_state & UIObjectFocusState::FOCUSED) != (focus_state & UIObjectFocusState::FOCUSED)) {
        UpdateCursor();
    }
}

void UITextbox::UpdateCursor()
{
    if (GetFocusState() & UIObjectFocusState::FOCUSED) {
        if (m_cursor_element == nullptr) {
            m_cursor_element = CreateUIObject<UIPanel>(NAME("TextboxCursor"), Vec2i { 0, 0 }, UIObjectSize({ 1, UIObjectSize::PIXEL }, { 90, UIObjectSize::PERCENT }));
            m_cursor_element->SetBackgroundColor(Vec4f { 0, 0, 0, 1 }); // black

            UIObject::AddChildUIObject(m_cursor_element);
        }
    } else {
        if (m_cursor_element != nullptr) {
            UIObject::RemoveChildUIObject(m_cursor_element);

            m_cursor_element = nullptr;
        }
    }

    m_cursor_blink_blend_var.SetValue(1.0f);
    m_cursor_blink_blend_var.SetTarget(1.0f);
}

Color UITextbox::GetPlaceholderTextColor() const
{
    // Vec4f background_color_rgba = Vec4f(ComputeBlendedBackgroundColor());
    // background_color_rgba *= background_color_rgba.w;
    // background_color_rgba.w = 1.0f;

    // const float brightness = (0.299f * background_color_rgba.x) + (0.587f * background_color_rgba.y) + (0.114f * background_color_rgba.z);

    // if (brightness < 0.35f) {
    //     return Color(Vec4f { 1.0f, 1.0f, 1.0f, 0.8f });
    // } else {
    //     return Color(Vec4f { 0.0f, 0.0f, 0.0f, 0.8f });
    // }

    return Color(Vec4f(Vec4f(GetTextColor()).GetXYZ(), 0.5f));
}

void UITextbox::UpdateTextColor()
{
    if (!m_text_element) {
        return;
    }

    if (ShouldDisplayPlaceholder()) {
        m_text_element->SetText(m_placeholder);
        m_text_element->SetTextColor(GetPlaceholderTextColor());
    } else {
        m_text_element->SetText(m_text);
        m_text_element->SetTextColor(GetTextColor());
    }
}

#pragma region UITextbox

} // namespace hyperion
