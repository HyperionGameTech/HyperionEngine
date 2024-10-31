/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIPanel.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>

#include <core/containers/Bitset.hpp>

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

    if (Bitset bitset { uint64(uint8(orientation)) }; bitset.Count() != 1) {
        for (Bitset::BitIndex bit_index : bitset) {
            SetIsScrollEnabled(UIObjectScrollbarOrientation(Bitset::GetBitMask(bit_index)), is_scroll_enabled);
            
            return;
        }
    }

    RC<UIObject> *scrollbar = nullptr;
    UIObjectSize scrollbar_size;
    UIObjectAlignment scrollbar_alignment = UIObjectAlignment::TOP_LEFT;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = &m_vertical_scrollbar;
        scrollbar_size = m_vertical_scrollbar
            ? m_vertical_scrollbar->GetSize()
            : UIObjectSize({ 20, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT });
        scrollbar_alignment = UIObjectAlignment::TOP_RIGHT;

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = &m_horizontal_scrollbar;
        scrollbar_size = m_horizontal_scrollbar
            ? m_horizontal_scrollbar->GetSize()
            : UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL });
        scrollbar_alignment = UIObjectAlignment::BOTTOM_LEFT;

        break;
    default:
        return;
    }

    m_is_scroll_enabled[orientation] = is_scroll_enabled;

    if (is_scroll_enabled) {
        // If scrollbar UIObject already exists and we are enabling scroll then return early
        if (*scrollbar != nullptr) {
            return;
        }

        m_on_scroll_handler = OnScroll.Bind([this](const MouseEvent &event_data) -> UIEventHandlerResult
        {
            return HandleScroll(event_data);
        });

        RC<UIPanel> new_scrollbar = GetStage()->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, scrollbar_size);
        new_scrollbar->SetAffectsParentSize(false);
        new_scrollbar->SetIsPositionAbsolute(true);
        new_scrollbar->SetIsScrollEnabled(UIObjectScrollbarOrientation::ALL, false);
        new_scrollbar->SetAcceptsFocus(false);
        new_scrollbar->SetParentAlignment(scrollbar_alignment);
        new_scrollbar->SetOriginAlignment(scrollbar_alignment);

        // testing
        new_scrollbar->SetBackgroundColor(Color(0xFF0000FFu));

        *scrollbar = std::move(new_scrollbar);

        UpdateScrollbarSize(orientation);
        UpdateScrollbarThumbPosition(orientation);

        UIObject::AddChildUIObject(*scrollbar);
    } else {
        m_on_scroll_handler.Reset();

        if (*scrollbar != nullptr) {
            UIObject::RemoveChildUIObject(scrollbar->Get());
            scrollbar->Reset();
        }
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

void UIPanel::OnScrollOffsetUpdate_Internal(Vec2f delta)
{
    HYP_SCOPE;

    // if (!MathUtil::ApproxEqual(delta.x, 0.0f)) {
    //     UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::HORIZONTAL);
    // }

    // if (!MathUtil::ApproxEqual(delta.y, 0.0f)) {
    //     UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::VERTICAL);
    // }


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

    if ((event_data.wheel.x != 0 && CanScroll(UIObjectScrollbarOrientation::HORIZONTAL)) || (event_data.wheel.y != 0 && CanScroll(UIObjectScrollbarOrientation::VERTICAL))) {
        HYP_LOG(UI, LogLevel::DEBUG, "Update scroll offset for {} to {}  inner: {}   size: {}", GetName(), GetScrollOffset(), GetActualInnerSize(), GetActualSize());
        
        SetScrollOffset(GetScrollOffset() - event_data.wheel * 10, /* smooth */ true);

        return UIEventHandlerResult::STOP_BUBBLING;
    }

    // allow parent to scroll
    return UIEventHandlerResult::OK;
}

void UIPanel::UpdateScrollbarSize(UIObjectScrollbarOrientation orientation)
{
    HYP_SCOPE;

    const Vec2i visible_content_size = GetActualSize();
    const Vec2i inner_content_size = GetActualInnerSize();
    const Vec2f ratios = Vec2f(visible_content_size) / Vec2f(inner_content_size);

    UIObject *scrollbar = nullptr;
    UIObjectSize scrollbar_inner_size;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = m_vertical_scrollbar.Get();
        scrollbar_inner_size = UIObjectSize({ 100, UIObjectSize::PERCENT }, { int(ratios.y * 100.0f), UIObjectSize::PERCENT });

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = m_horizontal_scrollbar.Get();
        scrollbar_inner_size = UIObjectSize({ int(ratios.x * 100.0f), UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT });

        break;
    default:
        HYP_UNREACHABLE();
    }

    AssertThrow(scrollbar != nullptr);

    RC<UIObject> scrollbar_inner = scrollbar->FindChildUIObject("ScrollbarInner", /* deep */ false);

    if (scrollbar_inner) {
        scrollbar_inner->SetSize(scrollbar_inner_size);
    } else {
        scrollbar_inner = GetStage()->CreateUIObject<UIButton>(NAME("ScrollbarInner"), Vec2i { 0, 0 }, scrollbar_inner_size);
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
        HYP_UNREACHABLE();
    }

    if (!m_is_scroll_enabled[orientation] || !scrollbar) {
        return;
    }

    RC<UIObject> scrollbar_inner = scrollbar->FindChildUIObject("ScrollbarInner", /* deep */ false);

    if (!scrollbar_inner) {
        return;
    }

    const Vec2i visible_content_size_offset = GetScrollOffset();
    const Vec2i inner_content_size = GetActualInnerSize() - GetActualSize();
    const Vec2f ratios = Vec2f(visible_content_size_offset) / Vec2f(inner_content_size);

    Vec2i position;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        position = Vec2i { 0, int(ratios.y * float(scrollbar->GetActualSize().y)) };

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        position = Vec2i { int(ratios.x * float(scrollbar->GetActualSize().x)), 0 };

        break;
    default:
        HYP_UNREACHABLE();
    }

    scrollbar_inner->SetPosition(position);
}

} // namespace hyperion
