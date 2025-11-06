#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region SDLAppContext Reflection Data

HYP_BEGIN_CLASS(SDLAppContext, 71, 0, NAME("AppContextBase"))
HYP_END_CLASS

#pragma endregion SDLAppContext Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AppContextBase Reflection Data

HYP_BEGIN_CLASS(AppContextBase, 70, 2, NAME("ObjectBase"))
HYP_END_CLASS

#pragma endregion AppContextBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SDLApplicationWindow Reflection Data

HYP_BEGIN_CLASS(SDLApplicationWindow, 74, 0, NAME("ApplicationWindow"))
HYP_END_CLASS

#pragma endregion SDLApplicationWindow Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Win32ApplicationWindow Reflection Data

HYP_BEGIN_CLASS(Win32ApplicationWindow, 75, 0, NAME("ApplicationWindow"))
HYP_END_CLASS

#pragma endregion Win32ApplicationWindow Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ApplicationWindow Reflection Data

HYP_BEGIN_CLASS(ApplicationWindow, 73, 2, NAME("ObjectBase"), ClassAttribute("abstract", true))
HYP_END_CLASS

#pragma endregion ApplicationWindow Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Win32AppContext Reflection Data

HYP_BEGIN_CLASS(Win32AppContext, 72, 0, NAME("AppContextBase"))
HYP_END_CLASS

#pragma endregion Win32AppContext Reflection Data

} // namespace hyperion

