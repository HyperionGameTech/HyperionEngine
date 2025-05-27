/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_WINDOW_HPP
#define HYPERION_UI_WINDOW_HPP

#include <ui/UIPanel.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/math/Vector2.hpp>

namespace hyperion {

#pragma region UIWindow

enum class UIWindowFlags : uint32
{
    NONE = 0x0,
    ALLOW_DRAG = 0x1,
    TITLE_BAR = 0x2,

    DEFAULT = ALLOW_DRAG | TITLE_BAR
};

HYP_MAKE_ENUM_FLAGS(UIWindowFlags)

HYP_CLASS()

class HYP_API UIWindow : public UIPanel
{
    HYP_OBJECT_BODY(UIWindow);

public:
    UIWindow();
    UIWindow(const UIWindow& other) = delete;
    UIWindow& operator=(const UIWindow& other) = delete;
    UIWindow(UIWindow&& other) noexcept = delete;
    UIWindow& operator=(UIWindow&& other) noexcept = delete;
    virtual ~UIWindow() override = default;

    HYP_FORCE_INLINE EnumFlags<UIWindowFlags> GetWindowFlags() const
    {
        return m_window_flags;
    }

    void SetWindowFlags(EnumFlags<UIWindowFlags> window_flags);

    virtual void SetText(const String& text) override;

    virtual void Init() override;

    virtual void AddChildUIObject(const RC<UIObject>& ui_object) override;
    virtual bool RemoveChildUIObject(UIObject* ui_object) override;

protected:
    virtual void UpdateSize_Internal(bool update_children) override;

    EnumFlags<UIWindowFlags> m_window_flags;

    RC<UIPanel> m_title_bar;
    RC<UIPanel> m_content;

private:
    Optional<Vec2i> m_mouse_drag_start;
};

#pragma endregion UIWindow

} // namespace hyperion

#endif