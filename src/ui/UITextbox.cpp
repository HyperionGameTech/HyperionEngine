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
    : m_textElement(nullptr),
      m_characterIndex(0),
      clearOnSubmit(false)
{
    SetBorderRadius(4);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding({ 5, 2 });
    SetTextColor(Color::Black());
    SetBackgroundColor(Vec4f::One());

    // For now
    SetIsScrollEnabled(SA_ALL, false);

    OnKeyDown
        .Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
            {
                // disable cursor blinking when typing
                m_cursorBlinkBlendVar.SetValue(1.0f);
                m_cursorBlinkBlendVar.SetTarget(1.0f);

                switch (eventData.keyCode)
                {
                case KeyCode::ARROW_LEFT:
                    if (m_characterIndex > 0)
                    {
                        --m_characterIndex;
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;
                case KeyCode::ARROW_RIGHT:
                    if (m_characterIndex < uint32(GetText().Length()))
                    {
                        ++m_characterIndex;
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;
                case KeyCode::RETURN:
                    SubmitTextChange();
                    Blur();

                    return UIEventHandlerResult::STOP_BUBBLING;
                case KeyCode::ESC:
                    SetText_Internal(m_prevText);
                    Blur();

                    return UIEventHandlerResult::STOP_BUBBLING;
                default:
                    break;
                }

                char keyChar;

                const bool shift = eventData.inputManager->IsShiftDown();
                const bool alt = eventData.inputManager->IsAltDown();
                const bool ctrl = eventData.inputManager->IsCtrlDown();

                if (KeyCodeToChar(eventData.keyCode, shift, alt, ctrl, keyChar))
                {
                    const String& text = GetText();

                    if (keyChar == '\b')
                    {
                        if (m_characterIndex > 0)
                        {
                            --m_characterIndex;

                            SetText_Internal(String(text.Substr(0, m_characterIndex)) + text.Substr(m_characterIndex + 1));
                        }
                    }
                    else if (keyChar == '\t')
                    {
                        // @TODO
                    }
                    else if (keyChar == '\n')
                    {
                        // @TODO
                    }
                    else if (keyChar == '\r')
                    {
                        // @TODO
                    }
                    else if (keyChar == '\f')
                    {
                        // @TODO
                    }
                    else if (keyChar == '\v')
                    {
                        // @TODO
                    }
                    else if (keyChar == '\a')
                    {
                        // @TODO
                    }
                    else if (keyChar == '\x1b')
                    {
                        // @TODO
                    }
                    else if (keyChar != '\0')
                    {
                        if (m_characterIndex > 0)
                        {
                            SetText_Internal(String(text.Substr(0, m_characterIndex)) + keyChar + text.Substr(m_characterIndex));
                        }
                        else
                        {
                            SetText_Internal(String::empty + keyChar + text);
                        }

                        ++m_characterIndex;
                    }
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    OnKeyUp.Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
               {
                   return UIEventHandlerResult::STOP_BUBBLING;
               })
        .Detach();
}

void UITextbox::Init()
{
    Threads::AssertOnThread(g_gameThread);

    UIPanel::Init();

    SetInnerSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
    // SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    Handle<UIText> textElement = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    textElement->SetTextSize(12.0f);
    textElement->SetAcceptsFocus(false);

    // m_textElement->SetAffectsParentSize(false);

    UIObject::AddChildUIObject(textElement);

    m_textElement = textElement;

    UpdateTextColor();
}

void UITextbox::SetTextColor(const Color& textColor)
{
    UIPanel::SetTextColor(textColor);

    UpdateTextColor();
}

void UITextbox::SetText(const String& text)
{
    HYP_SCOPE;

    m_prevText = GetText();

    SetText_Internal(text);

    SubmitTextChange();
}

void UITextbox::SetText_Internal(const String& text)
{
    const bool wasDisplayingPlaceholder = ShouldDisplayPlaceholder();

    UIObject::SetText(text);

    m_characterIndex = MathUtil::Min(m_characterIndex, uint32(text.Length()));

    if (m_textElement)
    {
        const bool shouldDisplayPlaceholder = ShouldDisplayPlaceholder();

        if (shouldDisplayPlaceholder != wasDisplayingPlaceholder)
        {
            UpdateTextColor();
        }

        m_textElement->SetText(shouldDisplayPlaceholder ? m_placeholder : text);
    }
}

void UITextbox::SetPlaceholder(const String& placeholder)
{
    m_placeholder = placeholder;

    UpdateTextColor();
}

void UITextbox::Update_Internal(float delta)
{
    UIPanel::Update_Internal(delta);

    if (m_cursorElement != nullptr)
    {
        if (!m_cursorBlinkBlendVar.Advance())
        {
            m_cursorBlinkBlendVar.SetTarget(1.0f - m_cursorBlinkBlendVar.GetTarget());
        }

        // animate cursor
        m_cursorElement->SetBackgroundColor(Vec4f { 0, 0, 0, m_cursorBlinkBlendVar.GetValue() });

        // update cursor position
        // Get pixel position of m_characterIndex
        Vec2i characterPosition = Vec2i(m_textElement->GetCharacterOffset(m_characterIndex));

        if (m_cursorElement->GetPosition() != characterPosition)
        {
            m_cursorElement->SetPosition(characterPosition);
        }
    }
}

void UITextbox::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState)
{
    const EnumFlags<UIObjectFocusState> previousFocusState = GetFocusState();

    UIPanel::SetFocusState_Internal(focusState);

    if ((previousFocusState & UIObjectFocusState::FOCUSED) != (focusState & UIObjectFocusState::FOCUSED))
    {
        const String& text = GetText();

        if (focusState & UIObjectFocusState::FOCUSED)
        {
            m_characterIndex = uint32(text.Length());
        }
        else
        {
            SubmitTextChange();
        }

        UpdateCursor();
    }
}

void UITextbox::UpdateCursor()
{
    if (GetFocusState() & UIObjectFocusState::FOCUSED)
    {
        if (m_cursorElement == nullptr)
        {
            m_cursorElement = CreateUIObject<UIPanel>(NAME("TextboxCursor"), Vec2i { 0, 0 }, UIObjectSize({ 1, UIObjectSize::PIXEL }, { 90, UIObjectSize::PERCENT }));
            m_cursorElement->SetBackgroundColor(Vec4f { 0, 0, 0, 1 }); // black
            m_cursorElement->SetAffectsParentSize(false);
            m_cursorElement->SetAllowMaterialUpdate(true); // set to dynamic material to reduce updates as we change the bg color

            UIObject::AddChildUIObject(m_cursorElement);
        }
    }
    else
    {
        if (m_cursorElement != nullptr)
        {
            UIObject::RemoveChildUIObject(m_cursorElement);

            m_cursorElement = nullptr;
        }
    }

    m_cursorBlinkBlendVar.SetValue(1.0f);
    m_cursorBlinkBlendVar.SetTarget(1.0f);
    m_cursorBlinkBlendVar.SetRate(60.0);
}

Color UITextbox::GetPlaceholderTextColor() const
{
    return Color(Vec4f(Vec4f(m_textColor).GetXYZ(), 0.5f));
}

void UITextbox::UpdateTextColor()
{
    if (!m_textElement)
    {
        return;
    }

    if (ShouldDisplayPlaceholder())
    {
        m_textElement->SetText(m_placeholder);
        m_textElement->SetTextColor(GetPlaceholderTextColor());
    }
    else
    {
        m_textElement->SetText(m_text);
        m_textElement->SetTextColor(m_textColor);
    }
}

void UITextbox::SubmitTextChange()
{
    HYP_SCOPE;

    const String& text = GetText();

    if (m_prevText != text)
    {
        OnTextChange(text);

        m_prevText = text;
        
        if (clearOnSubmit)
        {
            SetText_Internal("");
        }
    }
}

#pragma region UITextbox

} // namespace hyperion
