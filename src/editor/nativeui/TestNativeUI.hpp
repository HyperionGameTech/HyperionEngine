
#pragma once

#include <editor/EditorWindow.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class TestNativeUI : public EditorWindow
{
    HYP_OBJECT_BODY(TestNativeUI);

protected:
    virtual void Show_Internal() override;
};

} // namespace hyperion
