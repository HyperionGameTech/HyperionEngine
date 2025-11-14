/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/String.hpp>

#include <core/reflection/ObjectBase.hpp>

#include <core/math/Vector2.hpp>

#ifdef HYP_LIBUI
typedef struct uiWindow uiWindow;
#endif

namespace hyperion {

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

} // namespace hyperion
