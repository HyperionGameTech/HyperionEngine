#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region LightmapElement Reflection Data

HYP_BEGIN_STRUCT(LightmapElement, 388, 0, {}, HypClassAttribute("noscriptbindings", true))
    HypField(NAME(HYP_STR(Id)), &LightmapElement::id, offsetof(LightmapElement, id), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(OffsetUv)), &LightmapElement::offsetUv, offsetof(LightmapElement, offsetUv), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(OffsetCoords)), &LightmapElement::offsetCoords, offsetof(LightmapElement, offsetCoords), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Dimensions)), &LightmapElement::dimensions, offsetof(LightmapElement, dimensions), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Scale)), &LightmapElement::scale, offsetof(LightmapElement, scale), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(IsValid)), &LightmapElement::IsValid)
HYP_END_STRUCT

#pragma endregion LightmapElement Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LightmapVolumeAtlas Reflection Data

HYP_BEGIN_STRUCT(LightmapVolumeAtlas, 389, 0, {})
    HypProperty(NAME(HYP_STR(AtlasDimensions)), &LightmapVolumeAtlas::atlasDimensions),
    HypProperty(NAME(HYP_STR(Elements)), &LightmapVolumeAtlas::elements),
    HypProperty(NAME(HYP_STR(FreeSpaces)), &LightmapVolumeAtlas::freeSpaces)
HYP_END_STRUCT

#pragma endregion LightmapVolumeAtlas Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region LightmapVolume Reflection Data

HYP_BEGIN_CLASS(LightmapVolume, 178, 0, NAME("Entity"))
    HypMethod(NAME(HYP_STR(GetUUID)), &LightmapVolume::GetUUID, Span<const HypClassAttribute> { {HypClassAttribute("property", "UUID") } }),
    HypMethod(NAME(HYP_STR(GetAtlasTexture)), &LightmapVolume::GetAtlasTexture),
    HypField(NAME(HYP_STR(Uuid)), &LightmapVolume::m_uuid, offsetof(LightmapVolume, m_uuid), Span<const HypClassAttribute> { {HypClassAttribute("property", "UUID") } }),
    HypField(NAME(HYP_STR(RadianceAtlasTextures)), &LightmapVolume::m_radianceAtlasTextures, offsetof(LightmapVolume, m_radianceAtlasTextures), Span<const HypClassAttribute> { {HypClassAttribute("property", "RadianceAtlasTextures") } }),
    HypField(NAME(HYP_STR(IrradianceAtlasTextures)), &LightmapVolume::m_irradianceAtlasTextures, offsetof(LightmapVolume, m_irradianceAtlasTextures), Span<const HypClassAttribute> { {HypClassAttribute("property", "IrradianceAtlasTextures") } }),
    HypField(NAME(HYP_STR(Atlases)), &LightmapVolume::m_atlases, offsetof(LightmapVolume, m_atlases), Span<const HypClassAttribute> { {HypClassAttribute("property", "Atlases") } })
HYP_END_CLASS

#pragma endregion LightmapVolume Reflection Data

HYP_REGISTER_ENTITY_TYPE(LightmapVolume);
} // namespace hyperion


namespace hyperion {

#pragma region LightmapTextureType Reflection Data

HYP_BEGIN_ENUM(LightmapTextureType, 390, 0, {})
    HypConstant(NAME(HYP_STR(LTT_INVALID)), LightmapTextureType::LTT_INVALID),
    HypConstant(NAME(HYP_STR(LTT_RADIANCE)), LightmapTextureType::LTT_RADIANCE),
    HypConstant(NAME(HYP_STR(LTT_IRRADIANCE)), LightmapTextureType::LTT_IRRADIANCE),
    HypConstant(NAME(HYP_STR(LTT_MAX)), LightmapTextureType::LTT_MAX)
HYP_END_ENUM

#pragma endregion LightmapTextureType Reflection Data

} // namespace hyperion

