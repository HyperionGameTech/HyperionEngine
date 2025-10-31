#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EngineDriver Reflection Data

HYP_BEGIN_CLASS(EngineDriver, 195, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetInstance)), &EngineDriver::GetInstance),
    HypMethod(NAME(HYP_STR(GetCurrentWorld)), &EngineDriver::GetCurrentWorld),
    HypMethod(NAME(HYP_STR(SetCurrentWorld)), &EngineDriver::SetCurrentWorld),
    HypMethod(NAME(HYP_STR(GetDefaultWorld)), &EngineDriver::GetDefaultWorld)
HYP_END_CLASS

#pragma endregion EngineDriver Reflection Data

} // namespace hyperion

