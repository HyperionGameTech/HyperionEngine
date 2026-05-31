/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <UI/UIPanel.hpp>

#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

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
class ENGINE_API UIWindow : public UIPanel
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
        return m_windowFlags;
    }

    void SetWindowFlags(EnumFlags<UIWindowFlags> windowFlags);

    virtual void SetText(const String& text) override;

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;
    virtual bool RemoveChildUIObject(UIObject* uiObject) override;

protected:
    virtual void Init() override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    EnumFlags<UIWindowFlags> m_windowFlags;

    Handle<UIPanel> m_titleBar;
    Handle<UIPanel> m_content;

private:
    Optional<Vec2i> m_mouseDragStart;
};

#pragma endregion UIWindow

} // namespace Hyperion
