/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_BUTTON_HPP
#define HYPERION_UI_BUTTON_HPP

#include <ui/UIObject.hpp>

namespace hyperion {

class UIStage;
class UIText;

HYP_CLASS()
class HYP_API UIButton : public UIObject
{
    HYP_OBJECT_BODY(UIButton);

public:
    UIButton();
    UIButton(const UIButton &other)                 = delete;
    UIButton &operator=(const UIButton &other)      = delete;
    UIButton(UIButton &&other) noexcept             = delete;
    UIButton &operator=(UIButton &&other) noexcept  = delete;
    virtual ~UIButton() override                    = default;

    /*! \brief Sets the text of the button.
     * 
     * \param text The text of the button.
     */
    virtual void SetText(const String &text) override;

    /*! \brief Gets the text element of the button.
     * 
     * \return The text element of the button.
     */
    HYP_FORCE_INLINE const RC<UIText> &GetTextElement() const
        { return m_text_element; }

    virtual UIEventHandlerResult GetDefaultEventHandlerResult() const override
        {  return UIEventHandlerResult(UIEventHandlerResult::STOP_BUBBLING); }

    virtual void Init() override;

protected:
    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state) override;

    virtual Material::ParameterTable GetMaterialParameters() const override;

    RC<UIText>  m_text_element;
};

} // namespace hyperion

#endif