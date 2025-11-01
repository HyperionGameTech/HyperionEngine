#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region InputManager Reflection Data

HYP_BEGIN_CLASS(InputManager, 59, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(IsMouseLocked)), &InputManager::IsMouseLocked),
    HypMethod(NAME(HYP_STR(PushMouseLockState)), &InputManager::PushMouseLockState),
    HypMethod(NAME(HYP_STR(PopMouseLockState)), &InputManager::PopMouseLockState),
    HypMethod(NAME(HYP_STR(GetMousePosition)), &InputManager::GetMousePosition),
    HypMethod(NAME(HYP_STR(SetMousePosition)), &InputManager::SetMousePosition),
    HypMethod(NAME(HYP_STR(GetWindowSize)), &InputManager::GetWindowSize),
    HypMethod(NAME(HYP_STR(IsKeyDown)), &InputManager::IsKeyDown),
    HypMethod(NAME(HYP_STR(IsKeyUp)), &InputManager::IsKeyUp),
    HypMethod(NAME(HYP_STR(IsShiftDown)), &InputManager::IsShiftDown),
    HypMethod(NAME(HYP_STR(IsAltDown)), &InputManager::IsAltDown),
    HypMethod(NAME(HYP_STR(IsCtrlDown)), &InputManager::IsCtrlDown),
    HypMethod(NAME(HYP_STR(IsButtonDown)), &InputManager::IsButtonDown),
    HypMethod(NAME(HYP_STR(IsButtonUp)), &InputManager::IsButtonUp)
HYP_END_CLASS

#pragma endregion InputManager Reflection Data

} // namespace hyperion

