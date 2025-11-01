#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region ShadowMapAtlas Reflection Data

HYP_BEGIN_STRUCT(ShadowMapAtlas, 347, 0, {})
    HypProperty(NAME(HYP_STR(AtlasDimensions)), &ShadowMapAtlas::atlasDimensions),
    HypProperty(NAME(HYP_STR(Elements)), &ShadowMapAtlas::elements),
    HypProperty(NAME(HYP_STR(FreeSpaces)), &ShadowMapAtlas::freeSpaces),
    HypField(NAME(HYP_STR(AtlasIndex)), &ShadowMapAtlas::atlasIndex, offsetof(ShadowMapAtlas, atlasIndex), Span<const HypClassAttribute> { {HypClassAttribute("property", "AtlasIndex"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion ShadowMapAtlas Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShadowMapAtlasElement Reflection Data

HYP_BEGIN_STRUCT(ShadowMapAtlasElement, 348, 0, {})
HYP_END_STRUCT

#pragma endregion ShadowMapAtlasElement Reflection Data

} // namespace hyperion

