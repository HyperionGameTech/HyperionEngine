#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region LightmapElement Reflection Data

HYP_BEGIN_STRUCT(LightmapElement, 389, 0, {}, ClassAttribute("noscriptbindings", true))
    Field(NAME(HYP_STR(Id)), &LightmapElement::id, offsetof(LightmapElement, id), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(OffsetUv)), &LightmapElement::offsetUv, offsetof(LightmapElement, offsetUv), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(OffsetCoords)), &LightmapElement::offsetCoords, offsetof(LightmapElement, offsetCoords), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Dimensions)), &LightmapElement::dimensions, offsetof(LightmapElement, dimensions), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Scale)), &LightmapElement::scale, offsetof(LightmapElement, scale), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(IsValid)), &LightmapElement::IsValid)
HYP_END_STRUCT

#pragma endregion LightmapElement Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LightmapVolumeAtlas Reflection Data

HYP_BEGIN_STRUCT(LightmapVolumeAtlas, 390, 0, {})
    Property(NAME(HYP_STR(AtlasDimensions)), &LightmapVolumeAtlas::atlasDimensions),
    Property(NAME(HYP_STR(Elements)), &LightmapVolumeAtlas::elements),
    Property(NAME(HYP_STR(FreeSpaces)), &LightmapVolumeAtlas::freeSpaces)
HYP_END_STRUCT

#pragma endregion LightmapVolumeAtlas Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region LightmapVolume Reflection Data

HYP_BEGIN_CLASS(LightmapVolume, 178, 0, NAME("Entity"))
    Method(NAME(HYP_STR(GetUUID)), &LightmapVolume::GetUUID, Span<const ClassAttribute> { {ClassAttribute("property", "UUID") } }),
    Method(NAME(HYP_STR(GetAtlasTexture)), &LightmapVolume::GetAtlasTexture),
    Field(NAME(HYP_STR(Uuid)), &LightmapVolume::m_uuid, offsetof(LightmapVolume, m_uuid), Span<const ClassAttribute> { {ClassAttribute("property", "UUID") } }),
    Field(NAME(HYP_STR(RadianceAtlasTextures)), &LightmapVolume::m_radianceAtlasTextures, offsetof(LightmapVolume, m_radianceAtlasTextures), Span<const ClassAttribute> { {ClassAttribute("property", "RadianceAtlasTextures") } }),
    Field(NAME(HYP_STR(IrradianceAtlasTextures)), &LightmapVolume::m_irradianceAtlasTextures, offsetof(LightmapVolume, m_irradianceAtlasTextures), Span<const ClassAttribute> { {ClassAttribute("property", "IrradianceAtlasTextures") } }),
    Field(NAME(HYP_STR(Atlases)), &LightmapVolume::m_atlases, offsetof(LightmapVolume, m_atlases), Span<const ClassAttribute> { {ClassAttribute("property", "Atlases") } })
HYP_END_CLASS

#pragma endregion LightmapVolume Reflection Data

HYP_REGISTER_ENTITY_TYPE(LightmapVolume);
} // namespace hyperion


namespace hyperion {

#pragma region LightmapTextureType Reflection Data

HYP_BEGIN_ENUM(LightmapTextureType, 391, 0, {})
    StaticField(NAME(HYP_STR(LTT_INVALID)), LightmapTextureType::LTT_INVALID),
    StaticField(NAME(HYP_STR(LTT_RADIANCE)), LightmapTextureType::LTT_RADIANCE),
    StaticField(NAME(HYP_STR(LTT_IRRADIANCE)), LightmapTextureType::LTT_IRRADIANCE),
    StaticField(NAME(HYP_STR(LTT_MAX)), LightmapTextureType::LTT_MAX)
HYP_END_ENUM

#pragma endregion LightmapTextureType Reflection Data

} // namespace hyperion

