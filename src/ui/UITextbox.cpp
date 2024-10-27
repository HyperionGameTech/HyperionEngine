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

    // OnScroll.RemoveAll();
    // OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
    // {
    //     HYP_LOG(UI, LogLevel::INFO, "Scrolling textbox: {}", event_data.wheel);

    //     SetScrollOffset(GetScrollOffset() - event_data.wheel * 5, /* smooth */ false);

    //     return UIEventHandlerResult::STOP_BUBBLING;
    // }).Detach();

    OnKeyDown.Bind([this](const KeyboardEvent &event_data) -> UIEventHandlerResult
    {
        char key_char;

        const bool shift = event_data.input_manager->IsShiftDown();
        const bool alt = event_data.input_manager->IsAltDown();
        const bool ctrl = event_data.input_manager->IsCtrlDown();

        if (KeyCodeToChar(event_data.key_code, shift, alt, ctrl, key_char)) {
            HYP_LOG(UI, LogLevel::INFO, "Textbox keydown: char = {}", key_char);

            const String &text = GetText();

            if (key_char == '\b') {
                SetText(text.Substr(0, text.Length() - 2));
            } else {
                SetText(text + key_char);
            }

            return UIEventHandlerResult::STOP_BUBBLING;
        }

        return UIEventHandlerResult::OK;
    }).Detach();
}

void UITextbox::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    UIPanel::SetBackgroundColor(Vec4f::One());

    SetInnerSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
    // SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    m_text_element = GetStage()->CreateUIObject<UIText>(NAME("TextboxText"), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    m_text_element->SetText(m_text);
    m_text_element->SetTextSize(12.0f);
    m_text_element->SetTextColor(Vec4f { 0, 0, 0, 1 }); // black
    // m_text_element->SetAffectsParentSize(false);

    UIObject::AddChildUIObject(m_text_element);
}

void UITextbox::SetText(const String &text)
{
    UIObject::SetText(text);

    if (m_text_element) {
        m_text_element->SetText(text);
    }
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
        if (focus_state & UIObjectFocusState::FOCUSED) {
            SetBackgroundColor(Vec4f { 0.8f, 0.8f, 0.8f, 1.0f });
        } else {
            SetBackgroundColor(Vec4f::One());
        }

        UpdateCursor();
    }
}

void UITextbox::UpdateCursor()
{
    if (GetFocusState() & UIObjectFocusState::FOCUSED) {
        if (m_cursor_element == nullptr) {
            m_cursor_element = GetStage()->CreateUIObject<UIPanel>(NAME("TextboxCursor"), Vec2i { 0, 0 }, UIObjectSize({ 1, UIObjectSize::PIXEL }, { 90, UIObjectSize::PERCENT }));
            m_cursor_element->SetBackgroundColor(Vec4f { 0, 0, 0, 1 }); // black

            HYP_LOG(UI, LogLevel::DEBUG, "Textbox size before adding cursor element {}, inner size: {}, parent size: {}, cursor size: {}", GetActualSize(), GetActualInnerSize(), GetParentUIObject()->GetActualSize(), m_cursor_element->GetActualSize());
            UIObject::AddChildUIObject(m_cursor_element);

            HYP_LOG(UI, LogLevel::DEBUG, "Textbox size after adding cursor element {}, inner size: {}, parent size: {}, cursor size: {}", GetActualSize(), GetActualInnerSize(), GetParentUIObject()->GetActualSize(), m_cursor_element->GetActualSize());
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

#pragma region UITextbox

} // namespace hyperion
