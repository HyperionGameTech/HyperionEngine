/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_PANEL_HPP
#define HYPERION_UI_PANEL_HPP

#include <ui/UIObject.hpp>

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
    HYP_FORCE_INLINE bool IsScrollEnabled() const
        { return m_is_scroll_enabled; }

    HYP_METHOD()
    void SetIsScrollEnabled(bool is_scroll_enabled);

    virtual bool IsScrollable() const override
    {
        return m_is_scroll_enabled
            && (GetActualInnerSize().x > GetActualSize().x || GetActualInnerSize().y > GetActualSize().y);
    }

    virtual bool IsContainer() const override
        { return true; }

    virtual void Init() override;

    virtual void AddChildUIObject(const RC<UIObject> &ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

protected:
    virtual void UpdateSize_Internal(bool update_children) override;

    virtual void OnScrollOffsetUpdate_Internal() override;

    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::ParameterTable GetMaterialParameters() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;

private:
    void UpdateScrollbarSize();
    void UpdateScrollbarThumbPosition();

    UIEventHandlerResult HandleScroll(const MouseEvent &event_data);

    bool            m_is_scroll_enabled;
    DelegateHandler m_on_scroll_handler;

    RC<UIObject>    m_inner_content;
    RC<UIObject>    m_scrollbar;
};

#pragma endregion UIPanel

} // namespace hyperion

#endif