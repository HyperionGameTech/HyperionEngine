/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>

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

#ifdef HYP_LIBUI
    uiWindow* m_window = nullptr;
#endif
};

} // namespace hyperion
