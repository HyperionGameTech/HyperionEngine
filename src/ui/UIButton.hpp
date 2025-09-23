/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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
    UIButton(const UIButton& other) = delete;
    UIButton& operator=(const UIButton& other) = delete;
    UIButton(UIButton&& other) noexcept = delete;
    UIButton& operator=(UIButton&& other) noexcept = delete;
    virtual ~UIButton() override = default;

    /*! \brief Sets the text of the button.
     *
     * \param text The text of the button.
     */
    virtual void SetText(const String& text) override;

    /*! \brief Gets the text element of the button.
     *
     * \return The text element of the button.
     */
    HYP_FORCE_INLINE const Handle<UIText>& GetTextElement() const
    {
        return m_textElement;
    }

    virtual UIEventHandlerResult GetDefaultEventHandlerResult() const override
    {
        return UIEventHandlerResult(UIEventHandlerResult::STOP_BUBBLING);
    }

protected:
    virtual void Init() override;

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState) override;

    virtual MaterialParameters GetMaterialParameters() const override;

    Handle<UIText> m_textElement;
};

} // namespace hyperion

