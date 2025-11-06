#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EnvProbeType Reflection Data

HYP_BEGIN_ENUM(EnvProbeType, 356, 0, {})
    StaticField(NAME(HYP_STR(EPT_INVALID)), EnvProbeType::EPT_INVALID),
    StaticField(NAME(HYP_STR(EPT_SKY)), EnvProbeType::EPT_SKY),
    StaticField(NAME(HYP_STR(EPT_REFLECTION)), EnvProbeType::EPT_REFLECTION),
    StaticField(NAME(HYP_STR(EPT_SHADOW)), EnvProbeType::EPT_SHADOW),
    StaticField(NAME(HYP_STR(EPT_AMBIENT)), EnvProbeType::EPT_AMBIENT),
    StaticField(NAME(HYP_STR(EPT_MAX)), EnvProbeType::EPT_MAX)
HYP_END_ENUM

#pragma endregion EnvProbeType Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region EnvProbe Reflection Data

HYP_BEGIN_CLASS(EnvProbe, 135, 2, NAME("Entity"))
    Method(NAME(HYP_STR(GetEnvProbeType)), &EnvProbe::GetEnvProbeType),
    Method(NAME(HYP_STR(IsReflectionProbe)), &EnvProbe::IsReflectionProbe),
    Method(NAME(HYP_STR(IsSkyProbe)), &EnvProbe::IsSkyProbe),
    Method(NAME(HYP_STR(IsShadowProbe)), &EnvProbe::IsShadowProbe),
    Method(NAME(HYP_STR(IsAmbientProbe)), &EnvProbe::IsAmbientProbe),
    Method(NAME(HYP_STR(IsControlledByEnvGrid)), &EnvProbe::IsControlledByEnvGrid),
    Method(NAME(HYP_STR(GetAABB)), &EnvProbe::GetAABB),
    Method(NAME(HYP_STR(SetAABB)), &EnvProbe::SetAABB),
    Method(NAME(HYP_STR(GetOrigin)), &EnvProbe::GetOrigin),
    Method(NAME(HYP_STR(SetOrigin)), &EnvProbe::SetOrigin),
    Method(NAME(HYP_STR(GetCamera)), &EnvProbe::GetCamera),
    Field(NAME(HYP_STR(Aabb)), &EnvProbe::m_aabb, offsetof(EnvProbe, m_aabb), Span<const ClassAttribute> { {ClassAttribute("property", "AABB") } }),
    Field(NAME(HYP_STR(Dimensions)), &EnvProbe::m_dimensions, offsetof(EnvProbe, m_dimensions), Span<const ClassAttribute> { {ClassAttribute("property", "Dimensions") } }),
    Field(NAME(HYP_STR(EnvProbeType)), &EnvProbe::m_envProbeType, offsetof(EnvProbe, m_envProbeType), Span<const ClassAttribute> { {ClassAttribute("property", "EnvProbeType") } }),
    Field(NAME(HYP_STR(ShData)), &EnvProbe::m_shData, offsetof(EnvProbe, m_shData), Span<const ClassAttribute> { {ClassAttribute("property", "SHData") } })
HYP_END_CLASS

#pragma endregion EnvProbe Reflection Data

HYP_REGISTER_ENTITY_TYPE(EnvProbe);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region ReflectionProbe Reflection Data

HYP_BEGIN_CLASS(ReflectionProbe, 136, 0, NAME("EnvProbe"))
HYP_END_CLASS

#pragma endregion ReflectionProbe Reflection Data

HYP_REGISTER_ENTITY_TYPE(ReflectionProbe);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region SkyProbe Reflection Data

HYP_BEGIN_CLASS(SkyProbe, 137, 0, NAME("EnvProbe"))
    Method(NAME(HYP_STR(GetSkyboxCubemap)), &SkyProbe::GetSkyboxCubemap)
HYP_END_CLASS

#pragma endregion SkyProbe Reflection Data

HYP_REGISTER_ENTITY_TYPE(SkyProbe);
} // namespace hyperion


namespace hyperion {

#pragma region EnvProbeSphericalHarmonics Reflection Data

HYP_BEGIN_STRUCT(EnvProbeSphericalHarmonics, 357, 0, {}, ClassAttribute("serialize", "bitwise"))
HYP_END_STRUCT

#pragma endregion EnvProbeSphericalHarmonics Reflection Data

} // namespace hyperion

