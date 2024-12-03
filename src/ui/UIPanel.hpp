/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_PANEL_HPP
#define HYPERION_UI_PANEL_HPP

#include <ui/UIObject.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class UIStage;

#pragma region UIPanel

HYP_CLASS()
class HYP_API UIPanel : public UIObject
{
    HYP_OBJECT_BODY(UIPanel);

protected:
    UIPanel(UIObjectType type);

public:
    UIPanel();
    UIPanel(const UIPanel &other)                   = delete;
    UIPanel &operator=(const UIPanel &other)        = delete;
    UIPanel(UIPanel &&other) noexcept               = delete;
    UIPanel &operator=(UIPanel &&other) noexcept    = delete;
    virtual ~UIPanel() override                     = default;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsHorizontalScrollEnabled() const
        { return m_is_scroll_enabled & UIObjectScrollbarOrientation::HORIZONTAL; }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsVerticalScrollEnabled() const
        { return m_is_scroll_enabled & UIObjectScrollbarOrientation::VERTICAL; }

    HYP_METHOD()
    void SetIsScrollEnabled(UIObjectScrollbarOrientation orientation, bool is_scroll_enabled);

    virtual bool CanScroll(UIObjectScrollbarOrientation orientation) const override
    {
        const int orientation_index = UIObjectScrollbarOrientationToIndex(orientation);
        AssertThrow(orientation_index != -1);
        
        return (m_is_scroll_enabled & orientation)
            && GetActualInnerSize()[orientation_index] > GetActualSize()[orientation_index];
    }

    virtual void Init() override;

protected:
    virtual void UpdateSize_Internal(bool update_children) override;

    virtual void OnScrollOffsetUpdate_Internal(Vec2f delta) override;

    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::ParameterTable GetMaterialParameters() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;

private:
    void SetScrollbarVisible(UIObjectScrollbarOrientation orientation, bool visible);

    void UpdateScrollbarSize(UIObjectScrollbarOrientation orientation);
    void UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation orientation);

    UIEventHandlerResult HandleScroll(const MouseEvent &event_data);

    EnumFlags<UIObjectScrollbarOrientation> m_is_scroll_enabled;
    DelegateHandler                         m_on_scroll_handler;

    FixedArray<Vec2i, 2>                    m_initial_drag_position;
};

#pragma endregion UIPanel

} // namespace hyperion

#endif