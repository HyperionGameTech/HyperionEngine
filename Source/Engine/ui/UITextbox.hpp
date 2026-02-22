/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <ui/UIPanel.hpp>
#include <ui/UIText.hpp>

#include <core/containers/String.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API UITextbox : public UIPanel
{
    HYP_OBJECT_BODY(UITextbox);

public:
    UITextbox();
    UITextbox(const UITextbox& other) = delete;
    UITextbox& operator=(const UITextbox& other) = delete;
    UITextbox(UITextbox&& other) noexcept = delete;
    UITextbox& operator=(UITextbox&& other) noexcept = delete;
    virtual ~UITextbox() override = default;

    virtual void SetTextColor(const Color& textColor) override;

    /*! \brief Sets the text value of the textbox.
     *
     * \param text The text to set. */
    virtual void SetText(const String& text) override;

    /*! \brief Gets the placeholder text to display when the text is empty.
     *
     * \return The placeholder text. */
    HYP_METHOD(Property = "Placeholder", XMLAttribute = "placeholder")
    HYP_FORCE_INLINE const String& GetPlaceholder() const
    {
        return m_placeholder;
    }

    /*! \brief Sets the placeholder text to display when the text is empty.
     *
     * \param placeholder The placeholder text to set. */
    HYP_METHOD(Property = "Placeholder", XMLAttribute = "placeholder")
    void SetPlaceholder(const String& placeholder);

    HYP_METHOD()
    Color GetPlaceholderTextColor() const;

    HYP_FIELD()
    bool clearOnSubmit;

protected:
    virtual void Init() override;

    virtual bool NeedsUpdate() const override
    {
        return UIPanel::NeedsUpdate()
            || (GetFocusState() & UIObjectFocusState::FOCUSED); // need to update for cursor blinking
    }

    virtual void Update_Internal(float delta) override;

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState) override;

    virtual MaterialParameters GetMaterialParameters() const override;

    void UpdateCursor();
    void UpdateTextColor();

    bool ShouldDisplayPlaceholder() const
    {
        return GetText().Length() == 0 && m_placeholder.Length() != 0;
    }

    // Set text without broadcasting update
    void SetText_Internal(const String& text);
    void SubmitTextChange();

    UIText* m_textElement;
    Handle<UIObject> m_cursorElement;

    uint32 m_characterIndex;

    BlendVar<float> m_cursorBlinkBlendVar;

    String m_prevText;
    String m_placeholder;
};

} // namespace Hyperion
