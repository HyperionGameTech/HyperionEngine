/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIPanel.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>

#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UIPanel::UIPanel(UIObjectType type)
    : UIObject(type),
      m_is_scroll_enabled(true)
{
    SetBorderRadius(0);
    SetBackgroundColor(Color(0x101012FFu));
    SetTextColor(Color(0xFFFFFFFFu));

    m_on_scroll_handler = OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
    {
        return HandleScroll(event_data);
    });
}

UIPanel::UIPanel()
    : UIPanel(UIObjectType::PANEL)
{
}

void UIPanel::SetIsScrollEnabled(bool is_scroll_enabled)
{
    HYP_SCOPE;

    if ((m_scrollbar != nullptr) == is_scroll_enabled) {
        return;
    }

    m_is_scroll_enabled = is_scroll_enabled;

    if (m_scrollbar != nullptr) {
        UIObject::RemoveChildUIObject(m_scrollbar);
        m_scrollbar.Reset();
    }

    m_on_scroll_handler.Reset();

    if (is_scroll_enabled) {
        m_on_scroll_handler = OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
        {
            return HandleScroll(event_data);
        });

        RC<UIPanel> scrollbar = GetStage()->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 20, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));
        scrollbar->SetAffectsParentSize(false);
        scrollbar->SetIsScrollEnabled(false);
        scrollbar->SetAcceptsFocus(false);
        scrollbar->SetParentAlignment(UIObjectAlignment::TOP_RIGHT);
        scrollbar->SetOriginAlignment(UIObjectAlignment::TOP_RIGHT);

        // testing
        scrollbar->SetBackgroundColor(Color(0xFF0000FFu));

        m_scrollbar = std::move(scrollbar);

        UpdateScrollbarSize();
        UpdateScrollbarThumbPosition();

        UIObject::AddChildUIObject(m_scrollbar);
    }
}

void UIPanel::Init()
{
    HYP_SCOPE;

    UIObject::Init();

    m_inner_content = GetStage()->CreateUIObject<UIObject>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::FILL }, { 100, UIObjectSize::FILL }));
    UIObject::AddChildUIObject(m_inner_content);
}

void UIPanel::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;

    m_inner_content->AddChildUIObject(ui_object);
}

bool UIPanel::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;
    
    return m_inner_content->RemoveChildUIObject(ui_object);
}

void UIPanel::UpdateSize_Internal(bool update_children)
{
    HYP_SCOPE;

    UIObject::UpdateSize_Internal(update_children);

    const bool inner_size_greater = GetActualInnerSize().x > GetActualSize().x || GetActualInnerSize().y > GetActualSize().y;

    if (inner_size_greater) {
        if (m_is_scroll_enabled) {
            if (m_scrollbar == nullptr) {
                SetIsScrollEnabled(true);
            } else {
                UpdateScrollbarSize();
                UpdateScrollbarThumbPosition();
            }
        }
    } else {
        if (!m_is_scroll_enabled || m_scrollbar != nullptr) {
            SetIsScrollEnabled(false);
        }
    }
}

void UIPanel::OnScrollOffsetUpdate_Internal()
{
    HYP_SCOPE;

    UpdateScrollbarThumbPosition();
}

MaterialAttributes UIPanel::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

Material::ParameterTable UIPanel::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

Material::TextureSet UIPanel::GetMaterialTextures() const
{
    return UIObject::GetMaterialTextures();
}

UIEventHandlerResult UIPanel::HandleScroll(const MouseEvent &event_data)
{
    HYP_SCOPE;

    if (!IsScrollable()) {
        // allow parent to scroll
        return UIEventHandlerResult::OK;
    }

    HYP_LOG(UI, LogLevel::DEBUG, "Update scroll offset for {} to {}  inner: {}   size: {}", GetName(), GetScrollOffset(), GetActualInnerSize(), GetActualSize());
    
    SetScrollOffset(GetScrollOffset() - event_data.wheel * 10, /* smooth */ false);

    return UIEventHandlerResult::STOP_BUBBLING;
}

void UIPanel::UpdateScrollbarSize()
{
    HYP_SCOPE;

    RC<UIObject> scrollbar_inner = m_scrollbar->FindChildUIObject("ScrollbarInner", /* deep */ false);

    // @TODO Make size dependent on amount of content
    // @TODO Make work for horizontal scroll
    const Vec2i visible_content_size = GetActualSize();
    const Vec2i inner_content_size = GetActualInnerSize();

    const float visible_content_height_ratio = float(visible_content_size.y) / float(inner_content_size.y);

    HYP_LOG(UI, LogLevel::DEBUG, "Visible content ratio for {} : {} / {}  = {}", GetName(), visible_content_size, inner_content_size, visible_content_height_ratio);

    const UIObjectSize scrollbar_inner_size = UIObjectSize({ 100, UIObjectSize::PERCENT }, { int(visible_content_height_ratio * 100.0f), UIObjectSize::PERCENT });

    if (scrollbar_inner) {
        scrollbar_inner->SetSize(scrollbar_inner_size);
    } else {
        scrollbar_inner = GetStage()->CreateUIObject<UIButton>(NAME("ScrollbarInner"), Vec2i { 0, 0 }, scrollbar_inner_size);
        scrollbar_inner->SetNodeTag(NAME("TempTestKey"), NodeTag(String(GetName().LookupString())));
        scrollbar_inner->OnMouseDown.Bind([this, scrollbar_inner_weak = scrollbar_inner.ToWeak()](const MouseEvent &event_data) -> UIEventHandlerResult
        {
            // @TODO Implement scroll via drag
            if (RC<UIObject> scroll_inner = scrollbar_inner_weak.Lock()) {
                
            }

            return UIEventHandlerResult::STOP_BUBBLING;
        }).Detach();

        m_scrollbar->AddChildUIObject(scrollbar_inner);
    }
}

void UIPanel::UpdateScrollbarThumbPosition()
{
    HYP_SCOPE;

    if (!m_is_scroll_enabled || !m_scrollbar) {
        return;
    }

    RC<UIObject> scrollbar_inner = m_scrollbar->FindChildUIObject("ScrollbarInner", /* deep */ false);

    if (!scrollbar_inner) {
        return;
    }

    const Vec2i visible_content_size_offset = GetScrollOffset();
    const Vec2i inner_content_size = GetActualInnerSize() - GetActualSize();
    const Vec2f ratios = Vec2f(visible_content_size_offset) / Vec2f(inner_content_size);

    HYP_LOG(UI, LogLevel::DEBUG, "Scroll view offset for {} : {} / {} = {}", GetName(), visible_content_size_offset, inner_content_size, ratios * Vec2f(m_scrollbar->GetActualSize()));
    
    scrollbar_inner->SetPosition(Vec2i { 0, int(ratios.y * float(m_scrollbar->GetActualSize().y)) });

    HYP_LOG(UI, LogLevel::DEBUG, "Scrollbar thumb position for {} : {} {}", GetName(), scrollbar_inner->GetNodeTag(NAME("TempTestKey")).ToString(), scrollbar_inner->GetPosition());
}

} // namespace hyperion
