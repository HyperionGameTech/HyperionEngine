#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Material Reflection Data

HYP_BEGIN_CLASS(Material, 21, 0, NAME("AssetObject"))
    Method(NAME(HYP_STR(IsStatic)), &Material::IsStatic),
    Method(NAME(HYP_STR(IsDynamic)), &Material::IsDynamic),
    Method(NAME(HYP_STR(SetIsDynamic)), &Material::SetIsDynamic),
    Method(NAME(HYP_STR(Clone)), &Material::Clone),
    Field(NAME(HYP_STR(Parameters)), &Material::m_parameters, offsetof(Material, m_parameters)),
    Field(NAME(HYP_STR(Textures)), &Material::m_textures, offsetof(Material, m_textures)),
    Field(NAME(HYP_STR(Attributes)), &Material::m_attributes, offsetof(Material, m_attributes)),
    Field(NAME(HYP_STR(IsDynamic)), &Material::m_isDynamic, offsetof(Material, m_isDynamic))
HYP_END_CLASS

#pragma endregion Material Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialTextureKey Reflection Data

HYP_BEGIN_ENUM(MaterialTextureKey, 268, 0, {})
    StaticField(NAME(HYP_STR(NONE)), MaterialTextureKey::NONE),
    StaticField(NAME(HYP_STR(ALBEDO_MAP)), MaterialTextureKey::ALBEDO_MAP),
    StaticField(NAME(HYP_STR(NORMAL_MAP)), MaterialTextureKey::NORMAL_MAP),
    StaticField(NAME(HYP_STR(AO_MAP)), MaterialTextureKey::AO_MAP),
    StaticField(NAME(HYP_STR(PARALLAX_MAP)), MaterialTextureKey::PARALLAX_MAP),
    StaticField(NAME(HYP_STR(METALNESS_MAP)), MaterialTextureKey::METALNESS_MAP),
    StaticField(NAME(HYP_STR(ROUGHNESS_MAP)), MaterialTextureKey::ROUGHNESS_MAP),
    StaticField(NAME(HYP_STR(RADIANCE_MAP)), MaterialTextureKey::RADIANCE_MAP),
    StaticField(NAME(HYP_STR(IRRADIANCE_MAP)), MaterialTextureKey::IRRADIANCE_MAP),
    StaticField(NAME(HYP_STR(RESERVED0)), MaterialTextureKey::RESERVED0),
    StaticField(NAME(HYP_STR(RESERVED1)), MaterialTextureKey::RESERVED1),
    StaticField(NAME(HYP_STR(RESERVED2)), MaterialTextureKey::RESERVED2),
    StaticField(NAME(HYP_STR(RESERVED3)), MaterialTextureKey::RESERVED3),
    StaticField(NAME(HYP_STR(RESERVED4)), MaterialTextureKey::RESERVED4),
    StaticField(NAME(HYP_STR(RESERVED5)), MaterialTextureKey::RESERVED5),
    StaticField(NAME(HYP_STR(SPLAT_MAP)), MaterialTextureKey::SPLAT_MAP),
    StaticField(NAME(HYP_STR(BASE_TERRAIN_COLOR_MAP)), MaterialTextureKey::BASE_TERRAIN_COLOR_MAP),
    StaticField(NAME(HYP_STR(BASE_TERRAIN_NORMAL_MAP)), MaterialTextureKey::BASE_TERRAIN_NORMAL_MAP),
    StaticField(NAME(HYP_STR(BASE_TERRAIN_AO_MAP)), MaterialTextureKey::BASE_TERRAIN_AO_MAP),
    StaticField(NAME(HYP_STR(BASE_TERRAIN_PARALLAX_MAP)), MaterialTextureKey::BASE_TERRAIN_PARALLAX_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL1_COLOR_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_COLOR_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL1_NORMAL_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_NORMAL_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL1_AO_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_AO_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL1_PARALLAX_MAP)), MaterialTextureKey::TERRAIN_LEVEL1_PARALLAX_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL2_COLOR_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_COLOR_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL2_NORMAL_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_NORMAL_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL2_AO_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_AO_MAP),
    StaticField(NAME(HYP_STR(TERRAIN_LEVEL2_PARALLAX_MAP)), MaterialTextureKey::TERRAIN_LEVEL2_PARALLAX_MAP)
HYP_END_ENUM

#pragma endregion MaterialTextureKey Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialGroup Reflection Data

HYP_BEGIN_CLASS(MaterialGroup, 77, 0, NAME("HypObjectBase"))
HYP_END_CLASS

#pragma endregion MaterialGroup Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameterValue Reflection Data

HYP_BEGIN_STRUCT(MaterialParameterValue, 269, 0, {}, ClassAttribute("serialize", "bitwise"),ClassAttribute("size", 16))
HYP_END_STRUCT

#pragma endregion MaterialParameterValue Reflection Data

static_assert(sizeof(MaterialParameterValue) == 16, "Expected sizeof(MaterialParameterValue) to be 16 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameters Reflection Data

HYP_BEGIN_STRUCT(MaterialParameters, 270, 0, {})
    Field(NAME(HYP_STR(Values)), &MaterialParameters::m_values, offsetof(MaterialParameters, m_values), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MaterialParameters Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameterKey Reflection Data

HYP_BEGIN_ENUM(MaterialParameterKey, 271, 0, {})
    StaticField(NAME(HYP_STR(MATERIAL_KEY_NONE)), MaterialParameterKey::MATERIAL_KEY_NONE),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_ALBEDO)), MaterialParameterKey::MATERIAL_KEY_ALBEDO),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_METALNESS)), MaterialParameterKey::MATERIAL_KEY_METALNESS),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_ROUGHNESS)), MaterialParameterKey::MATERIAL_KEY_ROUGHNESS),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_TRANSMISSION)), MaterialParameterKey::MATERIAL_KEY_TRANSMISSION),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_EMISSIVE)), MaterialParameterKey::MATERIAL_KEY_EMISSIVE),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_SPECULAR)), MaterialParameterKey::MATERIAL_KEY_SPECULAR),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_SPECULAR_TINT)), MaterialParameterKey::MATERIAL_KEY_SPECULAR_TINT),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_ANISOTROPIC)), MaterialParameterKey::MATERIAL_KEY_ANISOTROPIC),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_SHEEN)), MaterialParameterKey::MATERIAL_KEY_SHEEN),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_SHEEN_TINT)), MaterialParameterKey::MATERIAL_KEY_SHEEN_TINT),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_CLEARCOAT)), MaterialParameterKey::MATERIAL_KEY_CLEARCOAT),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_CLEARCOAT_GLOSS)), MaterialParameterKey::MATERIAL_KEY_CLEARCOAT_GLOSS),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_SUBSURFACE)), MaterialParameterKey::MATERIAL_KEY_SUBSURFACE),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_NORMAL_MAP_INTENSITY)), MaterialParameterKey::MATERIAL_KEY_NORMAL_MAP_INTENSITY),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_UV_SCALE)), MaterialParameterKey::MATERIAL_KEY_UV_SCALE),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_PARALLAX_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_PARALLAX_HEIGHT),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_ALPHA_THRESHOLD)), MaterialParameterKey::MATERIAL_KEY_ALPHA_THRESHOLD),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_RESERVED2)), MaterialParameterKey::MATERIAL_KEY_RESERVED2),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT),
    StaticField(NAME(HYP_STR(MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT)), MaterialParameterKey::MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT)
HYP_END_ENUM

#pragma endregion MaterialParameterKey Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameterType Reflection Data

HYP_BEGIN_ENUM(MaterialParameterType, 272, 0, {})
    StaticField(NAME(HYP_STR(MPT_NONE)), MaterialParameterType::MPT_NONE),
    StaticField(NAME(HYP_STR(MPT_FLOAT)), MaterialParameterType::MPT_FLOAT),
    StaticField(NAME(HYP_STR(MPT_FLOAT2)), MaterialParameterType::MPT_FLOAT2),
    StaticField(NAME(HYP_STR(MPT_FLOAT3)), MaterialParameterType::MPT_FLOAT3),
    StaticField(NAME(HYP_STR(MPT_FLOAT4)), MaterialParameterType::MPT_FLOAT4),
    StaticField(NAME(HYP_STR(MPT_INT)), MaterialParameterType::MPT_INT),
    StaticField(NAME(HYP_STR(MPT_INT2)), MaterialParameterType::MPT_INT2),
    StaticField(NAME(HYP_STR(MPT_INT3)), MaterialParameterType::MPT_INT3),
    StaticField(NAME(HYP_STR(MPT_INT4)), MaterialParameterType::MPT_INT4)
HYP_END_ENUM

#pragma endregion MaterialParameterType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialTextures Reflection Data

HYP_BEGIN_STRUCT(MaterialTextures, 273, 0, {})
    Field(NAME(HYP_STR(Values)), &MaterialTextures::m_values, offsetof(MaterialTextures, m_values), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MaterialTextures Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MaterialParameter Reflection Data

HYP_BEGIN_STRUCT(MaterialParameter, 274, 0, {})
    Field(NAME(HYP_STR(Value)), &MaterialParameter::value, offsetof(MaterialParameter, value), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Type)), &MaterialParameter::type, offsetof(MaterialParameter, type), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MaterialParameter Reflection Data

} // namespace hyperion

