#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EnvProbeType Reflection Data

HYP_BEGIN_ENUM(EnvProbeType, 367, 0, {})
    HypConstant(NAME(HYP_STR(EPT_INVALID)), EnvProbeType::EPT_INVALID),
    HypConstant(NAME(HYP_STR(EPT_SKY)), EnvProbeType::EPT_SKY),
    HypConstant(NAME(HYP_STR(EPT_REFLECTION)), EnvProbeType::EPT_REFLECTION),
    HypConstant(NAME(HYP_STR(EPT_SHADOW)), EnvProbeType::EPT_SHADOW),
    HypConstant(NAME(HYP_STR(EPT_AMBIENT)), EnvProbeType::EPT_AMBIENT),
    HypConstant(NAME(HYP_STR(EPT_MAX)), EnvProbeType::EPT_MAX)
HYP_END_ENUM

#pragma endregion EnvProbeType Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region EnvProbe Reflection Data

HYP_BEGIN_CLASS(EnvProbe, 167, 2, NAME("Entity"))
    HypMethod(NAME(HYP_STR(GetEnvProbeType)), &EnvProbe::GetEnvProbeType),
    HypMethod(NAME(HYP_STR(IsReflectionProbe)), &EnvProbe::IsReflectionProbe),
    HypMethod(NAME(HYP_STR(IsSkyProbe)), &EnvProbe::IsSkyProbe),
    HypMethod(NAME(HYP_STR(IsShadowProbe)), &EnvProbe::IsShadowProbe),
    HypMethod(NAME(HYP_STR(IsAmbientProbe)), &EnvProbe::IsAmbientProbe),
    HypMethod(NAME(HYP_STR(IsControlledByEnvGrid)), &EnvProbe::IsControlledByEnvGrid),
    HypMethod(NAME(HYP_STR(GetAABB)), &EnvProbe::GetAABB),
    HypMethod(NAME(HYP_STR(SetAABB)), &EnvProbe::SetAABB),
    HypMethod(NAME(HYP_STR(GetOrigin)), &EnvProbe::GetOrigin),
    HypMethod(NAME(HYP_STR(SetOrigin)), &EnvProbe::SetOrigin),
    HypMethod(NAME(HYP_STR(GetCamera)), &EnvProbe::GetCamera),
    HypField(NAME(HYP_STR(Aabb)), &EnvProbe::m_aabb, offsetof(EnvProbe, m_aabb), Span<const HypClassAttribute> { {HypClassAttribute("property", "AABB") } }),
    HypField(NAME(HYP_STR(Dimensions)), &EnvProbe::m_dimensions, offsetof(EnvProbe, m_dimensions), Span<const HypClassAttribute> { {HypClassAttribute("property", "Dimensions") } }),
    HypField(NAME(HYP_STR(EnvProbeType)), &EnvProbe::m_envProbeType, offsetof(EnvProbe, m_envProbeType), Span<const HypClassAttribute> { {HypClassAttribute("property", "EnvProbeType") } }),
    HypField(NAME(HYP_STR(ShData)), &EnvProbe::m_shData, offsetof(EnvProbe, m_shData), Span<const HypClassAttribute> { {HypClassAttribute("property", "SHData") } })
HYP_END_CLASS

#pragma endregion EnvProbe Reflection Data

HYP_REGISTER_ENTITY_TYPE(EnvProbe);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region ReflectionProbe Reflection Data

HYP_BEGIN_CLASS(ReflectionProbe, 168, 0, NAME("EnvProbe"))
HYP_END_CLASS

#pragma endregion ReflectionProbe Reflection Data

HYP_REGISTER_ENTITY_TYPE(ReflectionProbe);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region SkyProbe Reflection Data

HYP_BEGIN_CLASS(SkyProbe, 169, 0, NAME("EnvProbe"))
    HypMethod(NAME(HYP_STR(GetSkyboxCubemap)), &SkyProbe::GetSkyboxCubemap)
HYP_END_CLASS

#pragma endregion SkyProbe Reflection Data

HYP_REGISTER_ENTITY_TYPE(SkyProbe);
} // namespace hyperion

