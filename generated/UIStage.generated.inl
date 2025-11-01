#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIStage Reflection Data

HYP_BEGIN_CLASS(UIStage, 213, 0, NAME("UIObject"))
    HypMethod(NAME(HYP_STR(GetSurfaceSize)), &UIStage::GetSurfaceSize),
    HypMethod(NAME(HYP_STR(SetSurfaceSize)), &UIStage::SetSurfaceSize),
    HypMethod(NAME(HYP_STR(GetScene)), &UIStage::GetScene),
    HypMethod(NAME(HYP_STR(SetScene)), &UIStage::SetScene),
    HypMethod(NAME(HYP_STR(GetCamera)), &UIStage::GetCamera)
HYP_END_CLASS

#pragma endregion UIStage Reflection Data

} // namespace hyperion

