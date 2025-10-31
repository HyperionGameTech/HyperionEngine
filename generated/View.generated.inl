#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region View Reflection Data

HYP_BEGIN_CLASS(View, 180, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetScenes)), &View::GetScenes),
    HypMethod(NAME(HYP_STR(AddScene)), &View::AddScene),
    HypMethod(NAME(HYP_STR(RemoveScene)), &View::RemoveScene),
    HypMethod(NAME(HYP_STR(GetCamera)), &View::GetCamera),
    HypMethod(NAME(HYP_STR(GetPriority)), &View::GetPriority),
    HypMethod(NAME(HYP_STR(SetPriority)), &View::SetPriority)
HYP_END_CLASS

#pragma endregion View Reflection Data

} // namespace hyperion

