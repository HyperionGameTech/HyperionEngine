#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region FirstPersonCameraInputHandler Reflection Data

HYP_BEGIN_CLASS(FirstPersonCameraInputHandler, 58, 0, NAME("InputHandlerBase"))
HYP_END_CLASS

#pragma endregion FirstPersonCameraInputHandler Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FirstPersonCameraControllerMode Reflection Data

HYP_BEGIN_ENUM(FirstPersonCameraControllerMode, 370, 0, {})
    HypConstant(NAME(HYP_STR(MOUSE_LOCKED)), FirstPersonCameraControllerMode::MOUSE_LOCKED),
    HypConstant(NAME(HYP_STR(MOUSE_FREE)), FirstPersonCameraControllerMode::MOUSE_FREE)
HYP_END_ENUM

#pragma endregion FirstPersonCameraControllerMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region FirstPersonCameraController Reflection Data

HYP_BEGIN_CLASS(FirstPersonCameraController, 172, 1, NAME("PerspectiveCameraController"))
    HypMethod(NAME(HYP_STR(GetMode)), &FirstPersonCameraController::GetMode, Span<const HypClassAttribute> { {HypClassAttribute("property", "Mode"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetMode)), &FirstPersonCameraController::SetMode, Span<const HypClassAttribute> { {HypClassAttribute("property", "Mode"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(IsMouseLockAllowed)), &FirstPersonCameraController::IsMouseLockAllowed)
HYP_END_CLASS

#pragma endregion FirstPersonCameraController Reflection Data

} // namespace hyperion

