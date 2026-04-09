/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/String.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/math/Vector2.hpp>

#ifdef HYP_LIBUI
typedef struct uiWindow uiWindow;
#endif

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class EditorWindow : public ObjectBase
{
    HYP_OBJECT_BODY(EditorWindow);

public:
    EditorWindow();
    virtual ~EditorWindow() override;

    void Show();

protected:
    virtual void Show_Internal() = 0;

    void SetTitle(const String& title);
    void SetWindowSize(const Vec2i& size);

    void Close();

#ifdef HYP_LIBUI
    uiWindow* m_window = nullptr;
#endif

private:
    String m_title;
    Vec2i m_windowSize;
};

} // namespace Hyperion
