#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UIStage Reflection Data

HYP_BEGIN_CLASS(UIStage, 214, 0, NAME("UIObject"))
    Method(NAME(HYP_STR(GetSurfaceSize)), &UIStage::GetSurfaceSize),
    Method(NAME(HYP_STR(SetSurfaceSize)), &UIStage::SetSurfaceSize),
    Method(NAME(HYP_STR(GetScene)), &UIStage::GetScene),
    Method(NAME(HYP_STR(SetScene)), &UIStage::SetScene),
    Method(NAME(HYP_STR(GetCamera)), &UIStage::GetCamera)
HYP_END_CLASS

#pragma endregion UIStage Reflection Data

} // namespace hyperion

