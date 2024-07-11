/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_TEXTBOX_HPP
#define HYPERION_UI_TEXTBOX_HPP

#include <ui/UIPanel.hpp>
#include <ui/UIText.hpp>

#include <core/containers/String.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class HYP_API UITextbox : public UIPanel
{
public:
    UITextbox(UIStage *stage, NodeProxy node_proxy);
    UITextbox(const UITextbox &other)                   = delete;
    UITextbox &operator=(const UITextbox &other)        = delete;
    UITextbox(UITextbox &&other) noexcept               = delete;
    UITextbox &operator=(UITextbox &&other) noexcept    = delete;
    virtual ~UITextbox() override                       = default;

    /*! \brief Sets the text value of the textbox.
     * 
     * \param text The text to set. */
    virtual void SetText(const String &text) override;

    virtual void Init() override;

protected:
    virtual void Update_Internal(GameCounter::TickUnit delta) override;
    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state) override;

    void UpdateCursor();

    RC<UIText>      m_text_element;
    RC<UIObject>    m_cursor_element;

    uint32          m_character_index;

    BlendVar<float> m_cursor_blink_blend_var;
};


} // namespace hyperion

#endif