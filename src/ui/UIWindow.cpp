/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIWindow.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIWindow

UIWindow::UIWindow()
    : m_windowFlags(UIWindowFlags::DEFAULT)
{
    SetBorderRadius(5);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding(Vec2i::Zero());
    SetBackgroundColor(Vec4f { 0.2f, 0.2f, 0.2f, 1.0f });
    SetDepth(1000);
    SetText("Window Title");
}

void UIWindow::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    UIPanel::Init();

    { // create title bar
        m_titleBar = CreateUIObject<UIPanel>(NAME("TitleBar"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 30, UIObjectSize::PIXEL }));
        m_titleBar->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
        m_titleBar->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
        m_titleBar->SetBorderRadius(5);
        m_titleBar->SetBorderFlags(UIObjectBorderFlags::TOP | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
        m_titleBar->SetPadding(Vec2i { 5, 5 });
        m_titleBar->SetBackgroundColor(Vec4f { 0.4f, 0.4f, 0.4f, 1.0f });

        Handle<UIText> titleBarText = CreateUIObject<UIText>(NAME("TitleBarText"), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        titleBarText->SetParentAlignment(UIObjectAlignment::CENTER);
        titleBarText->SetOriginAlignment(UIObjectAlignment::CENTER);
        titleBarText->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
        titleBarText->SetText(GetText());
        m_titleBar->AddChildUIObject(titleBarText);

        m_titleBar->OnMouseDown
            .Bind([this](const MouseEvent& event)
            {
                if (m_windowFlags & UIWindowFlags::ALLOW_DRAG)
                {
                    m_mouseDragStart = event.absolutePosition;

                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                return UIEventHandlerResult::OK;
            })
            .Detach();

        m_titleBar->OnMouseUp
            .Bind([this](const MouseEvent& event)
            {
                if (m_windowFlags & UIWindowFlags::ALLOW_DRAG)
                {
                    m_mouseDragStart.Unset();

                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                return UIEventHandlerResult::OK;
            })
            .Detach();

        m_titleBar->OnMouseDrag.Bind([this](const MouseEvent& event)
                                    {
                                        if (m_windowFlags & UIWindowFlags::ALLOW_DRAG)
                                        {
                                            if (m_mouseDragStart.HasValue())
                                            {
                                                Vec2i delta = event.absolutePosition - *m_mouseDragStart;
                                                SetPosition(GetPosition() + delta);

                                                m_mouseDragStart = event.absolutePosition;
                                            }

                                            return UIEventHandlerResult::STOP_BUBBLING;
                                        }

                                        return UIEventHandlerResult::OK;
                                    })
            .Detach();

        if (m_windowFlags & UIWindowFlags::TITLE_BAR)
        {
            UIPanel::AddChildUIObject(m_titleBar);
        }
    }

    m_content = CreateUIObject<UIPanel>(NAME("Content"), Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::FILL }));
    m_content->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    m_content->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    m_content->SetBorderRadius(5);
    m_content->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    m_content->SetPadding(Vec2i { 5, 5 });
    m_content->SetBackgroundColor(Vec4f::Zero()); // Transparent

    OnMouseDown.Bind([](...)
                   {
                       return UIEventHandlerResult::STOP_BUBBLING;
                   })
        .Detach();
    OnMouseUp.Bind([](...)
                 {
                     return UIEventHandlerResult::STOP_BUBBLING;
                 })
        .Detach();
    OnMouseDrag.Bind([](...)
                   {
                       return UIEventHandlerResult::STOP_BUBBLING;
                   })
        .Detach();
    OnMouseHover.Bind([](...)
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    })
        .Detach();
    OnMouseLeave.Bind([](...)
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    })
        .Detach();
    OnScroll.Bind([](...)
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                })
        .Detach();
    OnClick.Bind([](...)
               {
                   return UIEventHandlerResult::STOP_BUBBLING;
               })
        .Detach();
    OnKeyDown.Bind([](...)
                 {
                     return UIEventHandlerResult::STOP_BUBBLING;
                 })
        .Detach();
    OnKeyUp.Bind([](...)
               {
                   return UIEventHandlerResult::STOP_BUBBLING;
               })
        .Detach();

    UIPanel::AddChildUIObject(m_content);
}

void UIWindow::SetText(const String& text)
{
    HYP_SCOPE;

    if (m_titleBar != nullptr)
    {
        if (Handle<UIObject> titleBarText = m_titleBar->FindChildUIObject(NAME("TitleBarText")))
        {
            titleBarText->SetText(text);
        }
    }

    UIObject::SetText(text);
}

void UIWindow::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return;
    }

    m_content->AddChildUIObject(uiObject);

    UpdateSize(false);
}

bool UIWindow::RemoveChildUIObject(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return false;
    }

    if (m_content->RemoveChildUIObject(uiObject))
    {
        UpdateSize(false);

        return true;
    }

    return UIObject::RemoveChildUIObject(uiObject);
}

void UIWindow::UpdateSize_Internal(bool updateChildren)
{
    HYP_SCOPE;

    UIPanel::UpdateSize_Internal(updateChildren);
}

void UIWindow::SetWindowFlags(EnumFlags<UIWindowFlags> windowFlags)
{
    HYP_SCOPE;

    if (m_windowFlags == windowFlags)
    {
        return;
    }

    if ((windowFlags & UIWindowFlags::TITLE_BAR) != m_windowFlags)
    {
        if (windowFlags & UIWindowFlags::TITLE_BAR)
        {
            UIPanel::AddChildUIObject(m_titleBar);
        }
        else
        {
            UIPanel::RemoveChildUIObject(m_titleBar);
        }
    }

    m_windowFlags = windowFlags;
}

#pragma region UIWindow

} // namespace hyperion
