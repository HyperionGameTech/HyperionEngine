#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EngineDriver Reflection Data

HYP_BEGIN_CLASS(EngineDriver, 196, 0, NAME("HypObjectBase"))
    Method(NAME(HYP_STR(GetInstance)), &EngineDriver::GetInstance),
    Method(NAME(HYP_STR(GetCurrentWorld)), &EngineDriver::GetCurrentWorld),
    Method(NAME(HYP_STR(SetCurrentWorld)), &EngineDriver::SetCurrentWorld),
    Method(NAME(HYP_STR(GetDefaultWorld)), &EngineDriver::GetDefaultWorld)
HYP_END_CLASS

#pragma endregion EngineDriver Reflection Data

} // namespace hyperion

