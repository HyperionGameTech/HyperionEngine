#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region DebugDrawer Reflection Data

HYP_BEGIN_CLASS(DebugDrawer, 118, 0, NAME("HypObjectBase"))
HYP_END_CLASS

#pragma endregion DebugDrawer Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region DebugDrawerConfig Reflection Data

HYP_BEGIN_STRUCT(DebugDrawerConfig, 324, 0, {}, HypClassAttribute("configname", "GlobalConfig"),HypClassAttribute("jsonpath", "Rendering.Debug.DebugDrawer"))
    HypField(NAME(HYP_STR(Enabled)), &DebugDrawerConfig::enabled, offsetof(DebugDrawerConfig, enabled), Span<const HypClassAttribute> { {HypClassAttribute("description", "Enable or disable the debug drawer.") } })
HYP_END_STRUCT

#pragma endregion DebugDrawerConfig Reflection Data

} // namespace hyperion

