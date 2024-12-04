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
    SetBackgroundColor(Color(0x1E1E1EFFu));
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

    m_is_scroll_enabled[orientation] = is_scroll_enabled;
    
    SetScrollbarVisible(orientation, is_scroll_enabled);
}

void UIPanel::SetScrollbarVisible(UIObjectScrollbarOrientation orientation, bool visible)
{
    HYP_SCOPE;

    if (Bitset bitset { uint64(uint8(orientation)) }; bitset.Count() != 1) {
        for (Bitset::BitIndex bit_index : bitset) {
            SetScrollbarVisible(UIObjectScrollbarOrientation(Bitset::GetBitMask(bit_index)), visible);
            
            return;
        }
    }

    RC<UIObject> *scrollbar = nullptr;
    UIObjectSize scrollbar_size;
    UIObjectAlignment scrollbar_alignment = UIObjectAlignment::TOP_LEFT;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = &m_vertical_scrollbar;
        scrollbar_size = UIObjectSize({ GetVerticalScrollbarSize(), UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT });
        scrollbar_alignment = UIObjectAlignment::TOP_RIGHT;

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = &m_horizontal_scrollbar;
        scrollbar_size = UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetHorizontalScrollbarSize(), UIObjectSize::PIXEL });
        scrollbar_alignment = UIObjectAlignment::BOTTOM_LEFT;

        break;
    default:
        return;
    }

    if (visible) {
        // If scrollbar UIObject already exists and we are enabling scroll then return early
        if (*scrollbar != nullptr) {
            return;
        }

        RC<UIPanel> new_scrollbar = CreateUIObject<UIPanel>(Vec2i { 0, 0 }, scrollbar_size);
        new_scrollbar->SetAffectsParentSize(false);
        new_scrollbar->SetIsPositionAbsolute(true);
        new_scrollbar->SetIsScrollEnabled(UIObjectScrollbarOrientation::ALL, false);
        new_scrollbar->SetAcceptsFocus(false);
        new_scrollbar->SetParentAlignment(scrollbar_alignment);
        new_scrollbar->SetOriginAlignment(scrollbar_alignment);

        *scrollbar = std::move(new_scrollbar);

        UpdateScrollbarSize(orientation);
        UpdateScrollbarThumbPosition(orientation);

        UIObject::AddChildUIObject(*scrollbar);
    } else {
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

    if (m_is_scroll_enabled & UIObjectScrollbarOrientation::HORIZONTAL) {
        if (is_width_greater) {
            if (m_horizontal_scrollbar == nullptr) {
                SetScrollbarVisible(UIObjectScrollbarOrientation::HORIZONTAL, true);
            } else {
                UpdateScrollbarSize(UIObjectScrollbarOrientation::HORIZONTAL);
                UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::HORIZONTAL);
            }
        } else {
            SetScrollbarVisible(UIObjectScrollbarOrientation::HORIZONTAL, false);
        }
    }

    if (m_is_scroll_enabled & UIObjectScrollbarOrientation::VERTICAL) {
        if (is_height_greater) {
            if (m_vertical_scrollbar == nullptr) {
                SetScrollbarVisible(UIObjectScrollbarOrientation::VERTICAL, true);
            } else {
                UpdateScrollbarSize(UIObjectScrollbarOrientation::VERTICAL);
                UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::VERTICAL);
            }
        } else {
            SetScrollbarVisible(UIObjectScrollbarOrientation::VERTICAL, false);
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

    RC<UIObject> scrollbar;
    UIObjectSize thumb_size;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = m_vertical_scrollbar;
        thumb_size = UIObjectSize({ 100, UIObjectSize::PERCENT }, { int(ratios.y * 100.0f), UIObjectSize::PERCENT });

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = m_horizontal_scrollbar;
        thumb_size = UIObjectSize({ int(ratios.x * 100.0f), UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT });

        break;
    default:
        HYP_UNREACHABLE();
    }

    AssertThrow(scrollbar != nullptr);

    RC<UIObject> thumb = scrollbar->FindChildUIObject("ScrollbarThumb", /* deep */ false);

    if (thumb) {
        thumb->SetSize(thumb_size);
    } else {
        thumb = CreateUIObject<UIButton>(NAME("ScrollbarThumb"), Vec2i { 0, 0 }, thumb_size);

        thumb->OnMouseDown.Bind([this, orientation, thumb_weak = thumb.ToWeak()](const MouseEvent &event_data) -> UIEventHandlerResult
        {
            if (RC<UIObject> thumb = thumb_weak.Lock()) {
                const int orientation_index = UIObjectScrollbarOrientationToIndex(orientation);
                AssertThrow(orientation_index != -1);

                m_initial_drag_position[orientation_index] = Vec2i(event_data.position * Vec2f(thumb->GetActualSize()));
            }

            return UIEventHandlerResult::STOP_BUBBLING;
        }).Detach();

        thumb->OnMouseUp.Bind([this, orientation](const MouseEvent &event_data) -> UIEventHandlerResult
        {
            const int orientation_index = UIObjectScrollbarOrientationToIndex(orientation);
            AssertThrow(orientation_index != -1);

            m_initial_drag_position[orientation_index] = Vec2i::Zero();
            
            return UIEventHandlerResult::STOP_BUBBLING;
        }).Detach();

        thumb->OnMouseDrag.Bind([this, orientation, scrollbar_weak = scrollbar.ToWeak(), thumb_weak = thumb.ToWeak()](const MouseEvent &event_data) -> UIEventHandlerResult
        {
            if (CanScroll(orientation)) {
                RC<UIObject> thumb = thumb_weak.Lock();
                RC<UIObject> scrollbar = scrollbar_weak.Lock();

                if (thumb != nullptr && scrollbar != nullptr) {
                    const int orientation_index = UIObjectScrollbarOrientationToIndex(orientation);
                    AssertThrow(orientation_index != -1);

                    const Vec2f mouse_position_relative = Vec2f(event_data.absolute_position - m_initial_drag_position[orientation_index]) - scrollbar->GetAbsolutePosition();
                    const float mouse_relevant_position = mouse_position_relative[orientation_index];
                    
                    Vec2f ratios;
                    ratios[orientation_index] = mouse_relevant_position / float(scrollbar->GetActualSize()[orientation_index]);

                    SetScrollOffset(Vec2i(Vec2f(GetActualInnerSize()) * ratios), /* smooth */ false);
                }
            }

            return UIEventHandlerResult::STOP_BUBBLING;
        }).Detach();

        scrollbar->AddChildUIObject(thumb);
    }
}

void UIPanel::UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation orientation)
{
    HYP_SCOPE;

    RC<UIObject> scrollbar;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = m_vertical_scrollbar;

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = m_horizontal_scrollbar;

        break;
    default:
        HYP_UNREACHABLE();
    }

    if (!m_is_scroll_enabled[orientation] || !scrollbar) {
        return;
    }

    RC<UIObject> thumb = scrollbar->FindChildUIObject("ScrollbarThumb", /* deep */ false);

    if (!thumb) {
        return;
    }

    const Vec2i visible_content_size_offset = GetScrollOffset();
    const Vec2i inner_content_size = GetActualInnerSize() - GetActualSize();
    const Vec2f ratios = Vec2f(visible_content_size_offset) / Vec2f(inner_content_size);

    Vec2i position;

    switch (orientation) {
    case UIObjectScrollbarOrientation::VERTICAL:
        position = Vec2i { 0, int(ratios.y * float(scrollbar->GetActualSize().y - thumb->GetActualSize().y)) };

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        position = Vec2i { int(ratios.x * float(scrollbar->GetActualSize().x - thumb->GetActualSize().x)), 0 };

        break;
    default:
        HYP_UNREACHABLE();
    }

    thumb->SetPosition(position);
}

} // namespace hyperion
