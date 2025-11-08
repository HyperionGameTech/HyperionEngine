#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region View Reflection Data

HYP_BEGIN_CLASS(View, 179, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetScenes)), &View::GetScenes),
    Method(NAME(HYP_STR(AddScene)), &View::AddScene),
    Method(NAME(HYP_STR(RemoveScene)), &View::RemoveScene),
    Method(NAME(HYP_STR(GetCamera)), &View::GetCamera),
    Method(NAME(HYP_STR(GetPriority)), &View::GetPriority),
    Method(NAME(HYP_STR(SetPriority)), &View::SetPriority)
HYP_END_CLASS

#pragma endregion View Reflection Data

} // namespace hyperion

