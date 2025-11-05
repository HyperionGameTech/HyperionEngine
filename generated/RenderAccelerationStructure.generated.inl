#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region TLASBase Reflection Data

HYP_BEGIN_CLASS(TLASBase, 121, 1, NAME("HypObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion TLASBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region BLASBase Reflection Data

HYP_BEGIN_CLASS(BLASBase, 123, 1, NAME("HypObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion BLASBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AccelerationStructureType Reflection Data

HYP_BEGIN_ENUM(AccelerationStructureType, 330, 0, {})
    StaticField(NAME(HYP_STR(BOTTOM_LEVEL)), AccelerationStructureType::BOTTOM_LEVEL),
    StaticField(NAME(HYP_STR(TOP_LEVEL)), AccelerationStructureType::TOP_LEVEL)
HYP_END_ENUM

#pragma endregion AccelerationStructureType Reflection Data

} // namespace hyperion

