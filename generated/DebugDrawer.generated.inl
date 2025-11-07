#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region DebugDrawer Reflection Data

HYP_BEGIN_CLASS(DebugDrawer, 118, 0, NAME("ObjectBase"))
HYP_END_CLASS

#pragma endregion DebugDrawer Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DebugDrawerConfig Reflection Data

HYP_BEGIN_STRUCT(DebugDrawerConfig, 324, 0, {}, ClassAttribute("configname", "GlobalConfig"),ClassAttribute("jsonpath", "Rendering.Debug.DebugDrawer"))
    Field(NAME(HYP_STR(Enabled)), &DebugDrawerConfig::enabled, offsetof(DebugDrawerConfig, enabled), Span<const ClassAttribute> { {ClassAttribute("description", "Enable or disable the debug drawer.") } })
HYP_END_STRUCT

#pragma endregion DebugDrawerConfig Reflection Data

} // namespace hyperion

