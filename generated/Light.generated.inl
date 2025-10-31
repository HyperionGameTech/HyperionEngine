#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region DirectionalLight Reflection Data

HYP_BEGIN_CLASS(DirectionalLight, 171, 0, NAME("Light"))
    HypMethod(NAME(HYP_STR(GetDirection)), &DirectionalLight::GetDirection),
    HypMethod(NAME(HYP_STR(SetDirection)), &DirectionalLight::SetDirection)
HYP_END_CLASS

#pragma endregion DirectionalLight Reflection Data

HYP_REGISTER_ENTITY_TYPE(DirectionalLight);
} // namespace hyperion


namespace hyperion {

#pragma region LightFlags Reflection Data

HYP_BEGIN_ENUM(LightFlags, 379, 0, {})
    HypConstant(NAME(HYP_STR(LF_NONE)), LightFlags::LF_NONE),
    HypConstant(NAME(HYP_STR(LF_SHADOW)), LightFlags::LF_SHADOW),
    HypConstant(NAME(HYP_STR(LF_SHADOW_PCF)), LightFlags::LF_SHADOW_PCF),
    HypConstant(NAME(HYP_STR(LF_SHADOW_CONTACT_HARDENING)), LightFlags::LF_SHADOW_CONTACT_HARDENING),
    HypConstant(NAME(HYP_STR(LF_SHADOW_VSM)), LightFlags::LF_SHADOW_VSM),
    HypConstant(NAME(HYP_STR(LF_SHADOW_FILTER_MASK)), LightFlags::LF_SHADOW_FILTER_MASK),
    HypConstant(NAME(HYP_STR(LF_DEFAULT)), LightFlags::LF_DEFAULT)
HYP_END_ENUM

#pragma endregion LightFlags Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region AreaRectLight Reflection Data

HYP_BEGIN_CLASS(AreaRectLight, 172, 0, NAME("Light"))
HYP_END_CLASS

#pragma endregion AreaRectLight Reflection Data

HYP_REGISTER_ENTITY_TYPE(AreaRectLight);
} // namespace hyperion


namespace hyperion {

#pragma region LightType Reflection Data

HYP_BEGIN_ENUM(LightType, 380, 0, {})
    HypConstant(NAME(HYP_STR(LT_INVALID)), LightType::LT_INVALID),
    HypConstant(NAME(HYP_STR(LT_DIRECTIONAL)), LightType::LT_DIRECTIONAL),
    HypConstant(NAME(HYP_STR(LT_POINT)), LightType::LT_POINT),
    HypConstant(NAME(HYP_STR(LT_SPOT)), LightType::LT_SPOT),
    HypConstant(NAME(HYP_STR(LT_AREA_RECT)), LightType::LT_AREA_RECT),
    HypConstant(NAME(HYP_STR(LT_MAX)), LightType::LT_MAX)
HYP_END_ENUM

#pragma endregion LightType Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region SpotLight Reflection Data

HYP_BEGIN_CLASS(SpotLight, 173, 0, NAME("Light"))
HYP_END_CLASS

#pragma endregion SpotLight Reflection Data

HYP_REGISTER_ENTITY_TYPE(SpotLight);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region PointLight Reflection Data

HYP_BEGIN_CLASS(PointLight, 174, 0, NAME("Light"))
HYP_END_CLASS

#pragma endregion PointLight Reflection Data

HYP_REGISTER_ENTITY_TYPE(PointLight);
} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region Light Reflection Data

HYP_BEGIN_CLASS(Light, 170, 4, NAME("Entity"))
    HypMethod(NAME(HYP_STR(GetLightType)), &Light::GetLightType),
    HypMethod(NAME(HYP_STR(GetLightFlags)), &Light::GetLightFlags),
    HypMethod(NAME(HYP_STR(SetLightFlags)), &Light::SetLightFlags),
    HypMethod(NAME(HYP_STR(GetPosition)), &Light::GetPosition, Span<const HypClassAttribute> { {HypClassAttribute("property", "Position"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetPosition)), &Light::SetPosition, Span<const HypClassAttribute> { {HypClassAttribute("property", "Position"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetNormal)), &Light::GetNormal, Span<const HypClassAttribute> { {HypClassAttribute("property", "Normal"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetNormal)), &Light::SetNormal, Span<const HypClassAttribute> { {HypClassAttribute("property", "Normal"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetAreaSize)), &Light::GetAreaSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "AreaSize"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetAreaSize)), &Light::SetAreaSize, Span<const HypClassAttribute> { {HypClassAttribute("property", "AreaSize"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetColor)), &Light::GetColor, Span<const HypClassAttribute> { {HypClassAttribute("property", "Color"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetColor)), &Light::SetColor, Span<const HypClassAttribute> { {HypClassAttribute("property", "Color"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetIntensity)), &Light::GetIntensity, Span<const HypClassAttribute> { {HypClassAttribute("property", "Intensity"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetIntensity)), &Light::SetIntensity, Span<const HypClassAttribute> { {HypClassAttribute("property", "Intensity"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetRadius)), &Light::GetRadius, Span<const HypClassAttribute> { {HypClassAttribute("property", "Radius"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetRadius)), &Light::SetRadius, Span<const HypClassAttribute> { {HypClassAttribute("property", "Radius"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetFalloff)), &Light::GetFalloff, Span<const HypClassAttribute> { {HypClassAttribute("property", "Falloff"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetFalloff)), &Light::SetFalloff, Span<const HypClassAttribute> { {HypClassAttribute("property", "Falloff"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetSpotAngles)), &Light::GetSpotAngles, Span<const HypClassAttribute> { {HypClassAttribute("property", "SpotAngles"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetSpotAngles)), &Light::SetSpotAngles, Span<const HypClassAttribute> { {HypClassAttribute("property", "SpotAngles"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetMaterial)), &Light::GetMaterial, Span<const HypClassAttribute> { {HypClassAttribute("property", "Material"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetMaterial)), &Light::SetMaterial, Span<const HypClassAttribute> { {HypClassAttribute("property", "Material"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetShadowMapDimensions)), &Light::GetShadowMapDimensions, Span<const HypClassAttribute> { {HypClassAttribute("property", "ShadowMapDimensions"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetShadowMapDimensions)), &Light::SetShadowMapDimensions, Span<const HypClassAttribute> { {HypClassAttribute("property", "ShadowMapDimensions"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetAABB)), &Light::GetAABB),
    HypMethod(NAME(HYP_STR(GetShadowMapFilter)), &Light::GetShadowMapFilter, Span<const HypClassAttribute> { {HypClassAttribute("property", "ShadowMapFilter"), HypClassAttribute("editor", true), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(SetShadowMapFilter)), &Light::SetShadowMapFilter, Span<const HypClassAttribute> { {HypClassAttribute("property", "ShadowMapFilter"), HypClassAttribute("editor", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Type)), &Light::m_type, offsetof(Light, m_type)),
    HypField(NAME(HYP_STR(Flags)), &Light::m_flags, offsetof(Light, m_flags), Span<const HypClassAttribute> { {HypClassAttribute("property", "LightFlags") } })
HYP_END_CLASS

#pragma endregion Light Reflection Data

HYP_REGISTER_ENTITY_TYPE(Light);
} // namespace hyperion

