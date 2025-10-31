#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Material Reflection Data

HYP_BEGIN_CLASS(Material, 50, 0, NAME("AssetObject"))
    HypMethod(NAME(HYP_STR(IsStatic)), &Material::IsStatic),
    HypMethod(NAME(HYP_STR(IsDynamic)), &Material::IsDynamic),
    HypMethod(NAME(HYP_STR(SetIsDynamic)), &Material::SetIsDynamic),
    HypMethod(NAME(HYP_STR(Clone)), &Material::Clone),
    HypField(NAME(HYP_STR(Parameters)), &Material::m_parameters, offsetof(Material, m_parameters)),
    HypField(NAME(HYP_STR(Textures)), &Material::m_textures, offsetof(Material, m_textures)),
    HypField(NAME(HYP_STR(Attributes)), &Material::m_attributes, offsetof(Material, m_attributes)),
    HypField(NAME(HYP_STR(IsDynamic)), &Material::m_isDynamic, offsetof(Material, m_isDynamic))
HYP_END_CLASS

#pragma endregion Material Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialTextureKey Reflection Data

HYP_BEGIN_ENUM(MaterialTextureKey, 292, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), MaterialTextureKey::NONE),
    HypConstant(NAME(HYP_STR(ALBEDO_MAP)), MaterialTextureKey::ALBEDO_MAP),
    HypConstant(NAME(HYP_STR(NORMAL_MAP)), MaterialTextureKey::NORMAL_MAP),
    HypConstant(NAME(HYP_STR(AO_MAP)), MaterialTextureKey::AO_MAP),
    HypConstant(NAME(HYP_STR(PARALLAX_MAP)), MaterialTextureKey::PARALLAX_MAP),
    HypConstant(NAME(HYP_STR(METALNESS_MAP)), MaterialTextureKey::METALNESS_MAP),
    HypConstant(NAME(HYP_STR(ROUGHNESS_MAP)), MaterialTextureKey::ROUGHNESS_MAP),
    HypConstant(NAME(HYP_STR(RADIANCE_MAP)), MaterialTextureKey::RADIANCE_MAP),
    HypConstant(NAME(HYP_STR(IRRADIANCE_MAP)), MaterialTextureKey::IRRADIANCE_MAP),
    HypConstant(NAME(HYP_STR(RESERVED0)), MaterialTextureKey::RESERVED0),
    HypConstant(NAME(HYP_STR(RESERVED1)), MaterialTextureKey::RESERVED1),
    HypConstant(NAME(HYP_STR(RESERVED2)), MaterialTextureKey::RESERVED2),
    HypConstant(NAME(HYP_STR(RESERVED3)), MaterialTextureKey::RESERVED3),
    HypConstant(NAME(HYP_STR(RESERVED4)), MaterialTextureKey::RESERVED4),
    HypConstant(NAME(HYP_STR(RESERVED5)), MaterialTextureKey::RESERVED5),
    HypConstant(NAME(HYP_STR(SPLAT_MAP)), MaterialTextureKey::SPLAT_MAP),
    HypConstant(NAME(HYP_STR(BASE_TERRAIN_COLOR_MAP)), MaterialTextureKey::BASE_TERRAIN_COLOR_MAP),
    HypConstant(NAME(HYP_STR(BASE_TERRAIN_NORMAL_MAP)), MaterialTextureKey::BASE_TERRAIN_NORMAL_MAP),
    HypConstant(NAME(HYP_STR(BASE_TERRAIN_AO_MAP)), MaterialTextureKey::BASE_TERRAIN_AO_MAP),
    HypConstant(NAME(HYP_STR(BASE_TERRAIN_PARALLAX_MAP)), MaterialTextureKey::BASE_TERRAIN_PARALLAX_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL1_COLOR_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_COLOR_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL1_NORMAL_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_NORMAL_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL1_AO_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_AO_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL1_PARALLAX_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_PARALLAX_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL2_COLOR_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_COLOR_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL2_NORMAL_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_NORMAL_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL2_AO_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_AO_MAP),
    HypConstant(NAME(HYP_STR(TERRAIN_LEVEL2_PARALLAX_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_PARALLAX_MAP)
HYP_END_ENUM

#pragma endregion MaterialTextureKey Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialGroup Reflection Data

HYP_BEGIN_CLASS(MaterialGroup, 83, 0, NAME("HypObjectBase"))
HYP_END_CLASS

#pragma endregion MaterialGroup Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameterValue Reflection Data

HYP_BEGIN_STRUCT(MaterialParameterValue, 293, 0, {}, HypClassAttribute("serialize", "bitwise"),HypClassAttribute("size", 16))
HYP_END_STRUCT

#pragma endregion MaterialParameterValue Reflection Data

static_assert(sizeof(MaterialParameterValue) == 16, "Expected sizeof(MaterialParameterValue) to be 16 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameters Reflection Data

HYP_BEGIN_STRUCT(MaterialParameters, 294, 0, {})
    HypField(NAME(HYP_STR(Values)), &MaterialParameters::m_values, offsetof(MaterialParameters, m_values), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MaterialParameters Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameterKey Reflection Data

HYP_BEGIN_ENUM(MaterialParameterKey, 295, 0, {})
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_NONE)), MaterialParameterKey::MATERIAL_KEY_NONE),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_ALBEDO)), MaterialParameterKey::MATERIAL_KEY_ALBEDO),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_METALNESS)), MaterialParameterKey::MATERIAL_KEY_METALNESS),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_ROUGHNESS)), MaterialParameterKey::MATERIAL_KEY_ROUGHNESS),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_TRANSMISSION)), MaterialParameterKey::MATERIAL_KEY_TRANSMISSION),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_EMISSIVE)), MaterialParameterKey::MATERIAL_KEY_EMISSIVE),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_SPECULAR)), MaterialParameterKey::MATERIAL_KEY_SPECULAR),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_SPECULAR_TINT)), MaterialParameterKey::MATERIAL_KEY_SPECULAR_TINT),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_ANISOTROPIC)), MaterialParameterKey::MATERIAL_KEY_ANISOTROPIC),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_SHEEN)), MaterialParameterKey::MATERIAL_KEY_SHEEN),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_SHEEN_TINT)), MaterialParameterKey::MATERIAL_KEY_SHEEN_TINT),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_CLEARCOAT)), MaterialParameterKey::MATERIAL_KEY_CLEARCOAT),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_CLEARCOAT_GLOSS)), MaterialParameterKey::MATERIAL_KEY_CLEARCOAT_GLOSS),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_SUBSURFACE)), MaterialParameterKey::MATERIAL_KEY_SUBSURFACE),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_NORMAL_MAP_INTENSITY)), MaterialParameterKey::MATERIAL_KEY_NORMAL_MAP_INTENSITY),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_UV_SCALE)), MaterialParameterKey::MATERIAL_KEY_UV_SCALE),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_PARALLAX_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_PARALLAX_HEIGHT),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_ALPHA_THRESHOLD)), MaterialParameterKey::MATERIAL_KEY_ALPHA_THRESHOLD),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_RESERVED2)), MaterialParameterKey::MATERIAL_KEY_RESERVED2),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT),
    HypConstant(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT)
HYP_END_ENUM

#pragma endregion MaterialParameterKey Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameterType Reflection Data

HYP_BEGIN_ENUM(MaterialParameterType, 296, 0, {})
    HypConstant(NAME(HYP_STR(MPT_NONE)), MaterialParameterType::MPT_NONE),
    HypConstant(NAME(HYP_STR(MPT_FLOAT)), MaterialParameterType::MPT_FLOAT),
    HypConstant(NAME(HYP_STR(MPT_FLOAT2)), MaterialParameterType::MPT_FLOAT2),
    HypConstant(NAME(HYP_STR(MPT_FLOAT3)), MaterialParameterType::MPT_FLOAT3),
    HypConstant(NAME(HYP_STR(MPT_FLOAT4)), MaterialParameterType::MPT_FLOAT4),
    HypConstant(NAME(HYP_STR(MPT_INT)), MaterialParameterType::MPT_INT),
    HypConstant(NAME(HYP_STR(MPT_INT2)), MaterialParameterType::MPT_INT2),
    HypConstant(NAME(HYP_STR(MPT_INT3)), MaterialParameterType::MPT_INT3),
    HypConstant(NAME(HYP_STR(MPT_INT4)), MaterialParameterType::MPT_INT4)
HYP_END_ENUM

#pragma endregion MaterialParameterType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialTextures Reflection Data

HYP_BEGIN_STRUCT(MaterialTextures, 297, 0, {})
    HypField(NAME(HYP_STR(Values)), &MaterialTextures::m_values, offsetof(MaterialTextures, m_values), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MaterialTextures Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameter Reflection Data

HYP_BEGIN_STRUCT(MaterialParameter, 298, 0, {})
    HypField(NAME(HYP_STR(Value)), &MaterialParameter::value, offsetof(MaterialParameter, value), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Type)), &MaterialParameter::type, offsetof(MaterialParameter, type), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MaterialParameter Reflection Data

} // namespace hyperion

