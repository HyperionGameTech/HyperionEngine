#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region GpuTlasBase Reflection Data

HYP_BEGIN_CLASS(GpuTlasBase, 120, 1, NAME("ObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion GpuTlasBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GpuBlasBase Reflection Data

HYP_BEGIN_CLASS(GpuBlasBase, 122, 1, NAME("ObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion GpuBlasBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AccelerationStructureType Reflection Data

HYP_BEGIN_ENUM(AccelerationStructureType, 326, 0, {})
    StaticField(NAME(HYP_STR(BOTTOM_LEVEL)), AccelerationStructureType::BOTTOM_LEVEL),
    StaticField(NAME(HYP_STR(TOP_LEVEL)), AccelerationStructureType::TOP_LEVEL)
HYP_END_ENUM

#pragma endregion AccelerationStructureType Reflection Data

} // namespace hyperion

