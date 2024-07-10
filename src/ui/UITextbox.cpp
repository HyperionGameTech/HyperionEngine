/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UITextbox.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UITextbox

UITextbox::UITextbox(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::TEXTBOX),
      m_character_index(0)
{
    SetBorderRadius(2);
    SetPadding({ 5, 2 });

    OnScroll.RemoveAll();
    OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
    {
        HYP_LOG(UI, LogLevel::INFO, "Scrolling textbox: {}", event_data.wheel);

        SetScrollOffset(GetScrollOffset() - event_data.wheel * 5);

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();
}

void UITextbox::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    UIPanel::SetBackgroundColor(Vec4f::One());

    SetInnerSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));

    m_text_element = GetStage()->CreateUIObject<UIText>(NAME("TextboxText"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 12, UIObjectSize::PIXEL }));
    m_text_element->SetTextColor(Vec4f { 0, 0, 0, 1 }); // black
    m_text_element->SetText(m_text);
    m_text_element->SetAffectsParentSize(false);

    UIObject::AddChildUIObject(m_text_element);
}

void UITextbox::SetText(const String &text)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    m_text = text;

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
            m_cursor_element = GetStage()->CreateUIObject<UIPanel>(NAME("TextboxCursor"), Vec2i { 0, 0 }, UIObjectSize({ 1, UIObjectSize::PIXEL }, { 12, UIObjectSize::PIXEL }));
            m_cursor_element->SetBackgroundColor(Vec4f { 0, 0, 0, 1 }); // black

            AddChildUIObject(m_cursor_element);

            // TODO: Implement cursor blinking animation
        }
    } else {
        if (m_cursor_element != nullptr) {
            RemoveChildUIObject(m_cursor_element);

            m_cursor_element = nullptr;
        }
    }

    m_cursor_blink_blend_var.SetValue(1.0f);
    m_cursor_blink_blend_var.SetTarget(1.0f);
}

#pragma region UITextbox

} // namespace hyperion
