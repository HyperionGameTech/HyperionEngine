/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIWindow.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIWindow

UIWindow::UIWindow(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::WINDOW)
{
    SetBorderRadius(5);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding(Vec2i::Zero());
    SetBackgroundColor(Vec4f { 0.2f, 0.2f, 0.2f, 1.0f });
    SetDepth(1000);
}

void UIWindow::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    { // create title bar
        m_title_bar = GetStage()->CreateUIObject<UIPanel>(NAME("TitleBar"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
        m_title_bar->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
        m_title_bar->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
        m_title_bar->SetBorderRadius(5);
        m_title_bar->SetBorderFlags(UIObjectBorderFlags::TOP | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
        m_title_bar->SetPadding(Vec2i { 5, 5 });
        m_title_bar->SetBackgroundColor(Vec4f { 0.4f, 0.4f, 0.4f, 1.0f });

        RC<UIText> title_bar_text = GetStage()->CreateUIObject<UIText>(NAME("TitleBarText"), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        title_bar_text->SetParentAlignment(UIObjectAlignment::CENTER);
        title_bar_text->SetOriginAlignment(UIObjectAlignment::CENTER);
        title_bar_text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
        title_bar_text->SetText("Window Title");
        m_title_bar->AddChildUIObject(title_bar_text);

        m_title_bar->OnMouseDown.Bind([this](const MouseEvent &event)
        {
            m_mouse_drag_start.Set(event.absolute_position);

            HYP_LOG(UI, LogLevel::INFO, "Mouse down on title bar at: {}", m_mouse_drag_start.Get());

            return UIEventHandlerResult::STOP_BUBBLING;
        }).Detach();

        m_title_bar->OnMouseUp.Bind([this](const MouseEvent &event)
        {
            m_mouse_drag_start.Unset();

            return UIEventHandlerResult::STOP_BUBBLING;
        }).Detach();

        m_title_bar->OnMouseMove.Bind([this](const MouseEvent &event)
        {
            if (!m_mouse_drag_start.HasValue()) {
                return UIEventHandlerResult::OK;
            }

            Vec2i delta = event.absolute_position - m_mouse_drag_start.Get();
            SetPosition(GetPosition() + delta);

            m_mouse_drag_start.Set(event.absolute_position);

            return UIEventHandlerResult::STOP_BUBBLING;
        }).Detach();

        UIPanel::AddChildUIObject(m_title_bar);
    }

    m_content = GetStage()->CreateUIObject<UIPanel>(NAME("Content"), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::FILL }));
    m_content->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    m_content->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    m_content->SetBorderRadius(5);
    m_content->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    m_content->SetPadding(Vec2i { 5, 5 });
    m_content->SetBackgroundColor(Vec4f::Zero()); // Transparent
    UIPanel::AddChildUIObject(m_content);
}

void UIWindow::SetText(const String &text)
{
    HYP_SCOPE;

    if (m_title_bar != nullptr) {
        if (RC<UIObject> title_bar_text = m_title_bar->FindChildUIObject(NAME("TitleBarText"))) {
            title_bar_text->SetText(text);
        }
    }

    UIObject::SetText(text);
}

void UIWindow::AddChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return;
    }

    m_content->AddChildUIObject(ui_object);

    UpdateSize(false);
}

bool UIWindow::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }
    
    if (m_content->RemoveChildUIObject(ui_object)) {
        UpdateSize(false);

        return true;
    }

    return UIObject::RemoveChildUIObject(ui_object);
}

void UIWindow::UpdateSize(bool update_children)
{
    HYP_SCOPE;

    UIPanel::UpdateSize(update_children);
}

#pragma region UIWindow

} // namespace hyperion
