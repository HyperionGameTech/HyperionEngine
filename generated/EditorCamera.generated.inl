#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EditorCameraInputHandler Reflection Data

HYP_BEGIN_CLASS(EditorCameraInputHandler, 56, 0, NAME("InputHandlerBase"))
HYP_END_CLASS

#pragma endregion EditorCameraInputHandler Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorCameraControllerMode Reflection Data

HYP_BEGIN_ENUM(EditorCameraControllerMode, 257, 0, {})
    StaticField(NAME(HYP_STR(INACTIVE)), EditorCameraControllerMode::INACTIVE),
    StaticField(NAME(HYP_STR(FOCUSED)), EditorCameraControllerMode::FOCUSED),
    StaticField(NAME(HYP_STR(MOUSE_LOCKED)), EditorCameraControllerMode::MOUSE_LOCKED)
HYP_END_ENUM

#pragma endregion EditorCameraControllerMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EditorCameraController Reflection Data

HYP_BEGIN_CLASS(EditorCameraController, 174, 0, NAME("FirstPersonCameraController"))
HYP_END_CLASS

#pragma endregion EditorCameraController Reflection Data

} // namespace hyperion

