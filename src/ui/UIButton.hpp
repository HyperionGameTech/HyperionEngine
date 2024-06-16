/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_BUTTON_HPP
#define HYPERION_UI_BUTTON_HPP

#include <ui/UIObject.hpp>

namespace hyperion {

class UIStage;
class UIText;

class HYP_API UIButton : public UIObject
{
public:
    UIButton(UIStage *stage, NodeProxy node_proxy);
    UIButton(const UIButton &other)                 = delete;
    UIButton &operator=(const UIButton &other)      = delete;
    UIButton(UIButton &&other) noexcept             = delete;
    UIButton &operator=(UIButton &&other) noexcept  = delete;
    virtual ~UIButton() override                    = default;

    /*! \brief Gets the text of the button.
     * 
     * \return The text of the button.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    const String &GetText() const
        { return m_text; }

    /*! \brief Sets the text of the button.
     * 
     * \param text The text of the button.
     */
    void SetText(const String &text);

    /*! \brief Gets the text element of the button.
     * 
     * \return The text element of the button.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    const RC<UIText> &GetTextElement() const
        { return m_text_element; }

    virtual void Init() override;

protected:
    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state) override;

    virtual Handle<Material> GetMaterial() const override;

    String      m_text;
    RC<UIText>  m_text_element;
};

} // namespace hyperion

#endif