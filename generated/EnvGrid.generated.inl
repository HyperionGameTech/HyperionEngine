#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region EnvGrid Reflection Data

HYP_BEGIN_CLASS(EnvGrid, 133, 1, NAME("Entity"))
    HypMethod(NAME(HYP_STR(GetAABB)), &EnvGrid::GetAABB),
    HypField(NAME(HYP_STR(Aabb)), &EnvGrid::m_aabb, offsetof(EnvGrid, m_aabb), Span<const HypClassAttribute> { {HypClassAttribute("property", "AABB") } })
HYP_END_CLASS

#pragma endregion EnvGrid Reflection Data

HYP_REGISTER_ENTITY_TYPE(EnvGrid);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region LegacyEnvGrid Reflection Data

HYP_BEGIN_CLASS(LegacyEnvGrid, 134, 0, NAME("EnvGrid"))
    HypMethod(NAME(HYP_STR(SetAABB)), &LegacyEnvGrid::SetAABB),
    HypMethod(NAME(HYP_STR(GetView)), &LegacyEnvGrid::GetView),
    HypMethod(NAME(HYP_STR(GetCamera)), &LegacyEnvGrid::GetCamera),
    HypMethod(NAME(HYP_STR(Translate)), &LegacyEnvGrid::Translate)
HYP_END_CLASS

#pragma endregion LegacyEnvGrid Reflection Data

HYP_REGISTER_ENTITY_TYPE(LegacyEnvGrid);
} // namespace hyperion

