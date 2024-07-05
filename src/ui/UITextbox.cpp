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
    : UIPanel(parent, std::move(node_proxy), UIObjectType::TEXTBOX)
{
    SetBorderRadius(2);
    SetPadding({ 5, 2 });
}

void UITextbox::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    UIPanel::SetBackgroundColor(Vec4f::One());

    m_text_element = GetStage()->CreateUIObject<UIText>(NAME("TextboxText"), Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 12, UIObjectSize::PIXEL }));
    m_text_element->SetTextColor(Vec4f { 0, 0, 0, 1 }); // black
    m_text_element->SetText(m_text);
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

#pragma region UITextbox

} // namespace hyperion
