#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region FirstPersonCameraInputHandler Reflection Data

HYP_BEGIN_CLASS(FirstPersonCameraInputHandler, 59, 0, NAME("InputHandlerBase"))
HYP_END_CLASS

#pragma endregion FirstPersonCameraInputHandler Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FirstPersonCameraControllerMode Reflection Data

HYP_BEGIN_ENUM(FirstPersonCameraControllerMode, 366, 0, {})
    StaticField(NAME(HYP_STR(MOUSE_LOCKED)), FirstPersonCameraControllerMode::MOUSE_LOCKED),
    StaticField(NAME(HYP_STR(MOUSE_FREE)), FirstPersonCameraControllerMode::MOUSE_FREE)
HYP_END_ENUM

#pragma endregion FirstPersonCameraControllerMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FirstPersonCameraController Reflection Data

HYP_BEGIN_CLASS(FirstPersonCameraController, 172, 1, NAME("PerspectiveCameraController"))
    Method(NAME(HYP_STR(GetMode)), &FirstPersonCameraController::GetMode, Span<const ClassAttribute> { {ClassAttribute("property", "Mode"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetMode)), &FirstPersonCameraController::SetMode, Span<const ClassAttribute> { {ClassAttribute("property", "Mode"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(IsMouseLockAllowed)), &FirstPersonCameraController::IsMouseLockAllowed)
HYP_END_CLASS

#pragma endregion FirstPersonCameraController Reflection Data

} // namespace hyperion

