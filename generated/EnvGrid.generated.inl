#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region EnvGrid Reflection Data

HYP_BEGIN_CLASS(EnvGrid, 175, 1, NAME("Entity"))
    Method(NAME(HYP_STR(GetAABB)), &EnvGrid::GetAABB),
    Field(NAME(HYP_STR(Aabb)), &EnvGrid::m_aabb, offsetof(EnvGrid, m_aabb), Span<const ClassAttribute> { {ClassAttribute("property", "AABB") } })
HYP_END_CLASS

#pragma endregion EnvGrid Reflection Data

HYP_REGISTER_ENTITY_TYPE(EnvGrid);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region LegacyEnvGrid Reflection Data

HYP_BEGIN_CLASS(LegacyEnvGrid, 176, 0, NAME("EnvGrid"))
    Method(NAME(HYP_STR(SetAABB)), &LegacyEnvGrid::SetAABB),
    Method(NAME(HYP_STR(GetView)), &LegacyEnvGrid::GetView),
    Method(NAME(HYP_STR(GetCamera)), &LegacyEnvGrid::GetCamera),
    Method(NAME(HYP_STR(Translate)), &LegacyEnvGrid::Translate)
HYP_END_CLASS

#pragma endregion LegacyEnvGrid Reflection Data

HYP_REGISTER_ENTITY_TYPE(LegacyEnvGrid);
} // namespace hyperion

