#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region DirectionalLight Reflection Data

HYP_BEGIN_CLASS(DirectionalLight, 171, 0, NAME("Light"))
    Method(NAME(HYP_STR(GetDirection)), &DirectionalLight::GetDirection),
    Method(NAME(HYP_STR(SetDirection)), &DirectionalLight::SetDirection)
HYP_END_CLASS

#pragma endregion DirectionalLight Reflection Data

HYP_REGISTER_ENTITY_TYPE(DirectionalLight);
} // namespace hyperion


namespace hyperion {

#pragma region LightFlags Reflection Data

HYP_BEGIN_ENUM(LightFlags, 380, 0, {})
    StaticField(NAME(HYP_STR(LF_NONE)), LightFlags::LF_NONE),
    StaticField(NAME(HYP_STR(LF_SHADOW)), LightFlags::LF_SHADOW),
    StaticField(NAME(HYP_STR(LF_SHADOW_PCF)), LightFlags::LF_SHADOW_PCF),
    StaticField(NAME(HYP_STR(LF_SHADOW_CONTACT_HARDENING)), LightFlags::LF_SHADOW_CONTACT_HARDENING),
    StaticField(NAME(HYP_STR(LF_SHADOW_VSM)), LightFlags::LF_SHADOW_VSM),
    StaticField(NAME(HYP_STR(LF_SHADOW_FILTER_MASK)), LightFlags::LF_SHADOW_FILTER_MASK),
    StaticField(NAME(HYP_STR(LF_DEFAULT)), LightFlags::LF_DEFAULT)
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

HYP_BEGIN_ENUM(LightType, 381, 0, {})
    StaticField(NAME(HYP_STR(LT_INVALID)), LightType::LT_INVALID),
    StaticField(NAME(HYP_STR(LT_DIRECTIONAL)), LightType::LT_DIRECTIONAL),
    StaticField(NAME(HYP_STR(LT_POINT)), LightType::LT_POINT),
    StaticField(NAME(HYP_STR(LT_SPOT)), LightType::LT_SPOT),
    StaticField(NAME(HYP_STR(LT_AREA_RECT)), LightType::LT_AREA_RECT),
    StaticField(NAME(HYP_STR(LT_MAX)), LightType::LT_MAX)
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
    Method(NAME(HYP_STR(GetLightType)), &Light::GetLightType),
    Method(NAME(HYP_STR(GetLightFlags)), &Light::GetLightFlags),
    Method(NAME(HYP_STR(SetLightFlags)), &Light::SetLightFlags),
    Method(NAME(HYP_STR(GetPosition)), &Light::GetPosition, Span<const ClassAttribute> { {ClassAttribute("property", "Position"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetPosition)), &Light::SetPosition, Span<const ClassAttribute> { {ClassAttribute("property", "Position"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetNormal)), &Light::GetNormal, Span<const ClassAttribute> { {ClassAttribute("property", "Normal"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetNormal)), &Light::SetNormal, Span<const ClassAttribute> { {ClassAttribute("property", "Normal"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetAreaSize)), &Light::GetAreaSize, Span<const ClassAttribute> { {ClassAttribute("property", "AreaSize"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetAreaSize)), &Light::SetAreaSize, Span<const ClassAttribute> { {ClassAttribute("property", "AreaSize"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetColor)), &Light::GetColor, Span<const ClassAttribute> { {ClassAttribute("property", "Color"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetColor)), &Light::SetColor, Span<const ClassAttribute> { {ClassAttribute("property", "Color"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetIntensity)), &Light::GetIntensity, Span<const ClassAttribute> { {ClassAttribute("property", "Intensity"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetIntensity)), &Light::SetIntensity, Span<const ClassAttribute> { {ClassAttribute("property", "Intensity"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetRadius)), &Light::GetRadius, Span<const ClassAttribute> { {ClassAttribute("property", "Radius"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetRadius)), &Light::SetRadius, Span<const ClassAttribute> { {ClassAttribute("property", "Radius"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetFalloff)), &Light::GetFalloff, Span<const ClassAttribute> { {ClassAttribute("property", "Falloff"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetFalloff)), &Light::SetFalloff, Span<const ClassAttribute> { {ClassAttribute("property", "Falloff"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetSpotAngles)), &Light::GetSpotAngles, Span<const ClassAttribute> { {ClassAttribute("property", "SpotAngles"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetSpotAngles)), &Light::SetSpotAngles, Span<const ClassAttribute> { {ClassAttribute("property", "SpotAngles"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetMaterial)), &Light::GetMaterial, Span<const ClassAttribute> { {ClassAttribute("property", "Material"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetMaterial)), &Light::SetMaterial, Span<const ClassAttribute> { {ClassAttribute("property", "Material"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetShadowMapDimensions)), &Light::GetShadowMapDimensions, Span<const ClassAttribute> { {ClassAttribute("property", "ShadowMapDimensions"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetShadowMapDimensions)), &Light::SetShadowMapDimensions, Span<const ClassAttribute> { {ClassAttribute("property", "ShadowMapDimensions"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetAABB)), &Light::GetAABB),
    Method(NAME(HYP_STR(GetShadowMapFilter)), &Light::GetShadowMapFilter, Span<const ClassAttribute> { {ClassAttribute("property", "ShadowMapFilter"), ClassAttribute("editor", true), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(SetShadowMapFilter)), &Light::SetShadowMapFilter, Span<const ClassAttribute> { {ClassAttribute("property", "ShadowMapFilter"), ClassAttribute("editor", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Type)), &Light::m_type, offsetof(Light, m_type)),
    Field(NAME(HYP_STR(Flags)), &Light::m_flags, offsetof(Light, m_flags), Span<const ClassAttribute> { {ClassAttribute("property", "LightFlags") } })
HYP_END_CLASS

#pragma endregion Light Reflection Data

HYP_REGISTER_ENTITY_TYPE(Light);
} // namespace hyperion

