#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region InputManager Reflection Data

HYP_BEGIN_CLASS(InputManager, 57, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(IsMouseLocked)), &InputManager::IsMouseLocked),
    Method(NAME(HYP_STR(PushMouseLockState)), &InputManager::PushMouseLockState),
    Method(NAME(HYP_STR(PopMouseLockState)), &InputManager::PopMouseLockState),
    Method(NAME(HYP_STR(GetMousePosition)), &InputManager::GetMousePosition),
    Method(NAME(HYP_STR(SetMousePosition)), &InputManager::SetMousePosition),
    Method(NAME(HYP_STR(GetWindowSize)), &InputManager::GetWindowSize),
    Method(NAME(HYP_STR(IsKeyDown)), &InputManager::IsKeyDown),
    Method(NAME(HYP_STR(IsKeyUp)), &InputManager::IsKeyUp),
    Method(NAME(HYP_STR(IsShiftDown)), &InputManager::IsShiftDown),
    Method(NAME(HYP_STR(IsAltDown)), &InputManager::IsAltDown),
    Method(NAME(HYP_STR(IsCtrlDown)), &InputManager::IsCtrlDown),
    Method(NAME(HYP_STR(IsButtonDown)), &InputManager::IsButtonDown),
    Method(NAME(HYP_STR(IsButtonUp)), &InputManager::IsButtonUp)
HYP_END_CLASS

#pragma endregion InputManager Reflection Data

} // namespace hyperion

