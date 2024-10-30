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
      m_is_scroll_enabled(UIObjectScrollbarOrientation::ALL)
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

void UIPanel::SetIsScrollEnabled(UIObjectScrollbarOrientation orientation, bool is_scroll_enabled)
{
    HYP_SCOPE;

    RC<UIObject> *scrollbar = nullptr;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = &m_vertical_scrollbar;

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = &m_horizontal_scrollbar;

        break;
    default:
        return;
    }

    if ((*scrollbar != nullptr) == is_scroll_enabled) {
        return;
    }

    m_is_scroll_enabled[orientation] = is_scroll_enabled;

    if (*scrollbar != nullptr) {
        UIObject::RemoveChildUIObject(scrollbar->Get());
        scrollbar->Reset();
    }

    m_on_scroll_handler.Reset();

    if (is_scroll_enabled) {
        m_on_scroll_handler = OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
        {
            return HandleScroll(event_data);
        });

        RC<UIPanel> new_scrollbar = GetStage()->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 20, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));
        new_scrollbar->SetAffectsParentSize(false);
        new_scrollbar->SetIsScrollEnabled(UIObjectScrollbarOrientation::HORIZONTAL, false);
        new_scrollbar->SetIsScrollEnabled(UIObjectScrollbarOrientation::VERTICAL, false);
        new_scrollbar->SetAcceptsFocus(false);
        new_scrollbar->SetParentAlignment(UIObjectAlignment::TOP_RIGHT);
        new_scrollbar->SetOriginAlignment(UIObjectAlignment::TOP_RIGHT);

        // testing
        new_scrollbar->SetBackgroundColor(Color(0xFF0000FFu));

        *scrollbar = std::move(new_scrollbar);

        UpdateScrollbarSize(orientation);
        UpdateScrollbarThumbPosition(orientation);

        UIObject::AddChildUIObject(*scrollbar);
    }
}

void UIPanel::Init()
{
    HYP_SCOPE;

    UIObject::Init();
}

void UIPanel::UpdateSize_Internal(bool update_children)
{
    HYP_SCOPE;

    UIObject::UpdateSize_Internal(update_children);

    const bool is_width_greater = GetActualInnerSize().x > GetActualSize().x;
    const bool is_height_greater = GetActualInnerSize().y > GetActualSize().y;

    if (is_width_greater || is_height_greater) {
        if (m_is_scroll_enabled & UIObjectScrollbarOrientation::HORIZONTAL) {
            if (m_horizontal_scrollbar == nullptr) {
                SetIsScrollEnabled(UIObjectScrollbarOrientation::HORIZONTAL, true);
            } else if (is_width_greater) {
                UpdateScrollbarSize(UIObjectScrollbarOrientation::HORIZONTAL);
                UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::HORIZONTAL);
            }
        }

        if (m_is_scroll_enabled & UIObjectScrollbarOrientation::VERTICAL) {       
            if (m_vertical_scrollbar == nullptr) {
                SetIsScrollEnabled(UIObjectScrollbarOrientation::VERTICAL, true);
            } else if (is_height_greater) {
                UpdateScrollbarSize(UIObjectScrollbarOrientation::VERTICAL);
                UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::VERTICAL);
            }
        }
    } else {
        if (!(m_is_scroll_enabled & UIObjectScrollbarOrientation::HORIZONTAL) || m_horizontal_scrollbar != nullptr) {
            SetIsScrollEnabled(UIObjectScrollbarOrientation::HORIZONTAL, false);
        }

        if (!(m_is_scroll_enabled & UIObjectScrollbarOrientation::VERTICAL) || m_vertical_scrollbar != nullptr) {
            SetIsScrollEnabled(UIObjectScrollbarOrientation::VERTICAL, false);
        }
    }
}

void UIPanel::OnScrollOffsetUpdate_Internal()
{
    HYP_SCOPE;

    const Vec2i scroll_offset = GetScrollOffset();

    if (scroll_offset.x != 0) {
        UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::HORIZONTAL);
    }

    if (scroll_offset.y != 0) {
        UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::VERTICAL);
    }
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

void UIPanel::UpdateScrollbarSize(UIObjectScrollbarOrientation orientation)
{
    HYP_SCOPE;

    UIObject *scrollbar = nullptr;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = m_vertical_scrollbar.Get();

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = m_horizontal_scrollbar.Get();

        break;
    default:
        return;
    }

    AssertThrow(scrollbar != nullptr);

    RC<UIObject> scrollbar_inner = scrollbar->FindChildUIObject("ScrollbarInner", /* deep */ false);

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

        scrollbar->AddChildUIObject(scrollbar_inner);
    }
}

void UIPanel::UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation orientation)
{
    HYP_SCOPE;

    UIObject *scrollbar = nullptr;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = m_vertical_scrollbar.Get();

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = m_horizontal_scrollbar.Get();

        break;
    default:
        return;
    }

    if (!m_is_scroll_enabled[orientation] || !scrollbar) {
        return;
    }

    RC<UIObject> scrollbar_inner = scrollbar->FindChildUIObject("ScrollbarInner", /* deep */ false);

    if (!scrollbar_inner) {
        return;
    }

    // @TODO Make work for horizontal scroll

    const Vec2i visible_content_size_offset = GetScrollOffset();
    const Vec2i inner_content_size = GetActualInnerSize() - GetActualSize();
    const Vec2f ratios = Vec2f(visible_content_size_offset) / Vec2f(inner_content_size);

    HYP_LOG(UI, LogLevel::DEBUG, "Scroll view offset for {} : {} / {} = {}", GetName(), visible_content_size_offset, inner_content_size, ratios * Vec2f(scrollbar->GetActualSize()));
    
    scrollbar_inner->SetPosition(Vec2i { 0, int(ratios.y * float(scrollbar->GetActualSize().y)) });

    HYP_LOG(UI, LogLevel::DEBUG, "Scrollbar thumb position for {} : {} {}", GetName(), scrollbar_inner->GetNodeTag(NAME("TempTestKey")).ToString(), scrollbar_inner->GetPosition());
}

} // namespace hyperion
