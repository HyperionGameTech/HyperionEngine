#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EditorCameraInputHandler Reflection Data

HYP_BEGIN_CLASS(EditorCameraInputHandler, 56, 0, NAME("InputHandlerBase"))
HYP_END_CLASS

#pragma endregion EditorCameraInputHandler Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorCameraControllerMode Reflection Data

HYP_BEGIN_ENUM(EditorCameraControllerMode, 256, 0, {})
    HypConstant(NAME(HYP_STR(INACTIVE)), EditorCameraControllerMode::INACTIVE),
    HypConstant(NAME(HYP_STR(FOCUSED)), EditorCameraControllerMode::FOCUSED),
    HypConstant(NAME(HYP_STR(MOUSE_LOCKED)), EditorCameraControllerMode::MOUSE_LOCKED)
HYP_END_ENUM

#pragma endregion EditorCameraControllerMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorCameraController Reflection Data

HYP_BEGIN_CLASS(EditorCameraController, 173, 0, NAME("FirstPersonCameraController"))
HYP_END_CLASS

#pragma endregion EditorCameraController Reflection Data

} // namespace hyperion

